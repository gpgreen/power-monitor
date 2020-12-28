/*
 * power-monitor
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * This device will monitor a momentary pushbutton. If the button
 * is pushed, the device will enable a switching power supply.
 * This will turn on the Raspberry Pi. Once it is running, the Pi will
 * pull a pin hi and hold it there to show it is running. If the
 * button is pushed again, this device will pull a pin low that is monitored
 * by the Pi to signal a shutdown. Once the Pi has powered down
 * the pin that it has been holding hi will go low. This
 * allows this device to turn off the switching power supply.
 *
 * Device
 * ------
 * ATmega328P
 * signature = 0x1e9109
 * Fuse bits for ATmega328P
 *  Int RC osc 8.0MHz; Start-up time 6CK+0ms;[CKSEL=0010 SUT=00];
 *  Boot Flash section=256 words Boot start address=$3F00 [BOOTSZ=11]
 *  Enable Serial Program [SPIEN=0]
 *  Brown-out detection disabled
 *  Low=0xc2 [1100 0010] Hi=0xdf [1101 1111] Ext=0xFF
 *  from http://www.engbedded.com/fusecalc/
 *
 * to read fuses:
 * avrdude -c usbtiny -p atmega328p -U lfuse:r:lfuse.txt:h
 *    -U hfuse:r:hfuse.txt:h -U efuse:r:efuse.txt:h
 * to set fuses:
 * avrdude -c usbtiny -p atmega328p -U lfuse:w:0xc2:m
 *    -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "project.h"
#include "sensor.h"
#include "spi.h"

/*--------------------------------------------------------*/

typedef enum {
    Start,
    WaitEntry,
    Wait,
    SignaledOnEntry,
    SignaledOn,
    MCURunningEntry,
    MCURunning,
    SignaledOffEntry,
    SignaledOff,
    MCUOffEntry,
    MCUOff,
    SleepPowerDown,
    SleepPowerUp
} StateMachine;

/*--------------------------------------------------------
 globals
 --------------------------------------------------------*/

StateMachine machine_state;

// 1 when buttonpress detected, 0 otherwise
uint8_t buttonpress;
// 0 = up, 1 = down detected, waiting for delay
uint8_t button_state;
// button state mask updated by timer interrupt
// will be 0xFF when button up, 0x00 when down (debounced)
volatile uint8_t button_mask;
// number of timer interrupt while button is down
volatile uint8_t tovflows;
// number of timer interrupts for wake up period
volatile uint8_t wakeup_timer;

/*--------------------------------------------------------*/

void
init(void)
{
    // turn off modules that aren't used
    PRR |= (_BV(PRTWI)|_BV(PRTIM2)|_BV(PRTIM1)|_BV(PRUSART0));
    
    // timer set to CK/256, overflow interrupt enabled
    TCCR0B = _BV(CS02);
    TIMSK0 = _BV(TOIE0);

    // MCU_RUNNING, input, no pullup
    MCU_RUNNING_DIR &= ~(_BV(MCU_RUNNING));

    // BUTTON, input, no pullup
    BUTTON_DIR &= ~(_BV(BUTTON));

    // ENABLE, output, no pullup
    ENABLE_DIR |= _BV(ENABLE);

    // SHUTDOWN, output, no pullup
    SHUTDOWN_DIR |= _BV(SHUTDOWN);

#ifdef USE_LED
    // PORTD setup PINS for output
    DDRD |= (_BV(LED2)|_BV(LED3)|_BV(LED4));
    // PORTB setup PINS for output
    DDRB |= (_BV(LED1)|_BV(LED5)|_BV(LED6));
    // set unused ports as input and pull-up on
    DDRD &= ~(_BV(0)|_BV(1));
    PORTD |= (_BV(0)|_BV(1));
#else
    // set unused ports as input and pull-up on
    DDRD &= ~(_BV(0)|_BV(1)|_BV(5)|_BV(6)|_BV(7));
    PORTD |= (_BV(0)|_BV(1)|_BV(5)|_BV(6)|_BV(7));
    DDRB &= ~(_BV(0)|_BV(1));
    PORTB |= (_BV(0)|_BV(1));
#endif
    
    // enable inactive is low
    ENABLE_SET_OFF;

    // shutdown inactive is high
    SHUTDOWN_SET_ON;

    // the other components
    sensor_init();
    spi_init();
}

/* -- Transition Functions -- */

/*--------------------------------------------------------*/
// trigger when MCU activates MCU_RUNNING signal
inline
int mcu_is_running(void)
{
    return MCU_RUNNING_ON;
}

/*--------------------------------------------------------*/
// trigger when button pressed and released
inline
int button_pressed(void)
{
    if (buttonpress) {
        buttonpress = 0;
        return 1;
    }
    return 0;
}

/*--------------------------------------------------------*/
// trigger when the wake up period is over
inline
int wake_up_expired(void)
{
    // this is 750ms
    if (wakeup_timer >= (uint8_t)(F_CPU/256/256/1.5))
        return 1;
    return 0;
}

/*--------------------------------------------------------*/

int
main(void)
{
    // set the clock to 8MHz
//    CLKPR = _BV(CLKPCE);
//    CLKPR = 0;

    init();
    
    machine_state = Start;
    button_mask = 0xFF;
    
	// start interrupts
	sei();

    // main loop
    while(1)
    {
/*--------------------------------------------------------*/
        // check if button down, mask length is 66ms
        if (button_mask == 0x00 && button_state == 0) {
            button_state = 1;
            tovflows = 0;
            // has it been down for long enough
        } else if (button_state == 1) {
            if (button_mask == 0x0FF) {
                // is delay long enough, wait 200ms
                if (tovflows >= (F_CPU/256/256/5)) {
                    buttonpress = 1;
                }
                button_state = 0;
            }
        }
        
/*--------------------------------------------------------*/
        switch (machine_state) {
        case Start:
            wakeup_timer = 0;
            machine_state = WaitEntry;
            break;
/*--------------------------------------------------------*/
        case WaitEntry:
#ifdef USE_LED
            LED1_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
#endif
            ENABLE_SET_OFF;
            SHUTDOWN_SET_ON;
            machine_state = Wait;
            break;
        case Wait:
            if (button_pressed())
                machine_state = SignaledOnEntry;
            if (wake_up_expired())
                machine_state = MCUOffEntry;
            break;
/*--------------------------------------------------------*/
        case SignaledOnEntry:
#ifdef USE_LED
            LED2_SET_ON;
            LED1_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
#endif
            wakeup_timer = -1;
            ENABLE_SET_ON;
            machine_state = SignaledOn;
            break;
        case SignaledOn:
            if (mcu_is_running())
                machine_state = MCURunningEntry;
            if (button_pressed())
                machine_state = MCUOffEntry;
            break;
/*--------------------------------------------------------*/
        case MCURunningEntry:
#ifdef USE_LED
            LED3_SET_ON;
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED4_SET_OFF;
#endif
            machine_state = MCURunning;
            break;
        case MCURunning:
            if (button_pressed())
                machine_state = SignaledOffEntry;
            // if turned off via the desktop
            if (!mcu_is_running())
                machine_state = MCUOffEntry;
            break;
/*--------------------------------------------------------*/
        case SignaledOffEntry:
#ifdef USE_LED
            LED4_SET_ON;
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
#endif
            SHUTDOWN_SET_OFF;
            machine_state = SignaledOff;
            break;
        case SignaledOff:
            if (!mcu_is_running())
            {
                SHUTDOWN_SET_ON;
                machine_state = MCUOffEntry;
            }
            break;
/*--------------------------------------------------------*/
        case MCUOffEntry: case MCUOff:
#ifdef USE_LED
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
#endif
            ENABLE_SET_OFF;
            machine_state = SleepPowerDown;
            break;
/*--------------------------------------------------------*/
        case SleepPowerDown:
#ifdef USE_LED
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;
#endif
            // enter Power-Down mode
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);

            // SHUTDOWN pin to input and pull-up on
            SHUTDOWN_DIR &= ~(_BV(SHUTDOWN));
            SHUTDOWN_PORT |= _BV(SHUTDOWN);
            sensor_pre_power_down();
            spi_pre_power_down();
            
            // set INTO interrupt
            EIMSK |= _BV(INT0);

            // do the power down
            sleep_enable();
            sleep_mode();
            // woken up
            machine_state = SleepPowerUp;
            // NO BREAK
            // fall through to next state, so we don't
            // call other state machines
/*--------------------------------------------------------*/
        case SleepPowerUp:
            // turn off INT0 interrupt
            EIMSK &= ~(_BV(INT0));
            spi_post_power_down();
            sensor_post_power_down();
            // SHUTDOWN pin pull up off, to output
            SHUTDOWN_PORT &= ~(_BV(SHUTDOWN));
            SHUTDOWN_DIR |= _BV(SHUTDOWN);
            wakeup_timer = 0;
            machine_state = WaitEntry;
            break;
/*--------------------------------------------------------*/
        default:
            machine_state = Start;
        }
/*--------------------------------------------------------*/
        spi_state_machine();
        sensor_state_machine();
    }
    return 0;
}
/*--------------------------------------------------------*/

/*
 * Timer0 overflow interrupt
 * interrupt flag cleared by hardware
 */
ISR(TIMER0_OVF_vect)
{
    button_mask <<= 1;
    if (BUTTON_ON)
        button_mask |= 1;
    else
        button_mask &= ~1;
    if (button_state == 1)
        tovflows++;
    if (wakeup_timer >= 0)
        wakeup_timer++;
#ifdef USE_LED
    TOGGLE_LED6;
#endif
}

/*
 * INT0 interrupt
 * interrupt flag cleared by hardware
 */
ISR(INT0_vect)
{
    // does nothing but wake up the cpu
}
