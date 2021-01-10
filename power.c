/*
 * power-monitor
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * This device will monitor a momentary pushbutton. If the button is
 * pushed, the device will enable a switching power supply.  This will
 * turn on the Raspberry Pi. Once it is running, the Pi will pull a
 * pin hi (MCU_RUNNING) and hold it there to show it is running. If
 * the button is pushed again, this device will pull a pin hi
 * (SHUTDOWN) that is monitored by the Pi to signal a shutdown. Once
 * the Pi has powered down the pin (MCU_RUNNING) that it has been
 * holding hi will go low. This allows this device to turn off the
 * switching power supply.
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
#include <avr/wdt.h>
#include <util/atomic.h>

#include "project.h"
#include "power.h"
#include "sensor.h"
#include "spi.h"

/*--------------------------------------------------------
  globals
  --------------------------------------------------------*/

StateMachine machine_state;
StateMachine prev_state;

// button state mask updated by timer interrupt
// will be 0xFF when button up, 0x00 when down (debounced)
volatile uint8_t button_mask;

// number of timer interrupt while button is down
volatile int8_t button_timer;

// number of timer interrupts for wake up period
volatile int8_t wakeup_timer;

// number of timer interrupts for idle period
volatile int8_t idle_timer;

// flag for INT0
volatile uint8_t int0_event;

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
    DDRD |= (_BV(LED2)|_BV(LED3)|_BV(LED4)|_BV(LED6)|_BV(LED7));
    // PORTB setup PINS for output
    DDRB |= (_BV(LED1)|_BV(LED5));
    // unused pins input, pull-up on
    DDRD &= ~(_BV(3));
    PORTD |= _BV(3);
#else
    // set unused ports as input and pull-up on
    DDRD &= ~(_BV(0)|_BV(1)|_BV(3)|_BV(5)|_BV(6)|_BV(7));
    PORTD |= (_BV(0)|_BV(1)|_BV(3)|_BV(5)|_BV(6)|_BV(7));
#endif
    
    // enable inactive is low
    ENABLE_SET_OFF;

    // shutdown inactive is low
    SHUTDOWN_SET_OFF;

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
// trigger when button pressed
inline
int button_pressed(void)
{
    return button_mask == 0x00;
}

/*--------------------------------------------------------*/
// trigger when button released
inline
int button_released(void)
{
    return button_mask == 0xFF;
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
// trigger when the idle period is over
inline
int idle_expired(void)
{
    // this is 375ms
    if (idle_timer >= (uint8_t)(F_CPU/256/256/3))
        return 1;
    return 0;
}

/*--------------------------------------------------------*/
// determine interrupt source, no need for atomic
// as these are in priority order, if a higher priority
// happens, then it will get chosen
inline
WakeupEvent get_wakeup_event(void)
{
    if (int0_event)
        return ButtonEvt;
    if (spi_stc_event)
        return SPItxfer;
    if (adc_complete_event)
        return ADCcomplete;
    return Unknown;
}

/*--------------------------------------------------------*/
// change state
void
change_state(StateMachine new_state)
{
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        switch (machine_state) {
        case WaitEntry:
        case SignaledOnEntry:
        case MCURunningEntry:
        case ADCNoiseEntry:
        case SignaledOffEntry:
        case PowerDownEntry:
        case ButtonPress:
        case ButtonRelease:
            break;
        default:
            prev_state = machine_state;
        }
        machine_state = new_state;
    }
}

/*--------------------------------------------------------*/
// saved value of MCUSR register
uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

// turn off watchdog, get original value of MCUSR first
void get_mcusr(void) \
    __attribute__((naked)) \
    __attribute__((section(".init3")));

void get_mcusr(void)
{
    mcusr_mirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

/*--------------------------------------------------------*/
int
main(void)
{
    // set the clock to 8MHz
//    CLKPR = _BV(CLKPCE);
//    CLKPR = 0;

    init();
    
    machine_state = prev_state = Start;
    button_mask = 0xFF;
    wakeup_timer = -1;
    idle_timer = -1;
    button_timer = -1;
    WakeupEvent evt = Unknown;
    
	// start interrupts
	sei();

    // main loop
    while(1)
    {
/*--------------------------------------------------------*/
        switch (machine_state) {
        case Start:
            wakeup_timer = 0;
            change_state(WaitEntry);
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
            SHUTDOWN_SET_OFF;
            change_state(Wait);
            break;
        case Wait:
            if (button_pressed())
                change_state(ButtonPress);
            if (wake_up_expired())
                change_state(MCUOffEntry);
            break;
/*--------------------------------------------------------*/
        case ButtonPress:
            button_timer = 0;
            change_state(ButtonRelease);
            break;
        case ButtonRelease:
            if (button_released()) {
                // is delay long enough, wait 200ms
                if (button_timer >= (F_CPU/256/256/5)) {
                    if (prev_state == Wait)
                        change_state(SignaledOnEntry);
                    else if (prev_state == SignaledOn)
                        change_state(MCUOffEntry);
                    else if (prev_state == MCURunning)
                        change_state(SignaledOffEntry);
                } else {
                    // delay too short
                    if (prev_state == Wait)
                        change_state(WaitEntry);
                    else if (prev_state == SignaledOn)
                        change_state(SignaledOnEntry);
                    else if (prev_state == MCURunning)
                        change_state(MCURunningEntry);
                }
                button_timer = -1;
            }
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
            change_state(SignaledOn);
            break;
        case SignaledOn:
            if (mcu_is_running())
                change_state(MCURunningEntry);
            if (button_pressed())
                change_state(ButtonPress);
            break;
/*--------------------------------------------------------*/
        case MCURunningEntry:
#ifdef USE_LED
            LED3_SET_ON;
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED4_SET_OFF;
#endif
            idle_timer = 0;
            change_state(MCURunning);
            break;
        case MCURunning:
            if (button_pressed())
            {
                change_state(ButtonPress);
                idle_timer = -1;
            }
            // if turned off via the desktop
            if (!mcu_is_running())
            {
                change_state(MCUOffEntry);
                idle_timer = -1;
            }
            if (idle_expired())
            {
                change_state(ADCNoiseEntry);
                idle_timer = -1;
            }
            break;
/*--------------------------------------------------------*/
        case ADCNoiseEntry:
#ifdef USE_LED
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
#endif
            // set INTO interrupt
            EIMSK |= _BV(INT0);
            spi_pre_adc_noise();
            // enter Sleep mode
            set_sleep_mode(SLEEP_MODE_ADC);
            sleep_enable();
            sleep_mode();
            // get wakeup source
            evt = get_wakeup_event();
            change_state(ADCNoiseExit);
            break;
        case ADCNoiseExit:
            if (evt == ADCcomplete && mcu_is_running())
                change_state(ADCNoiseEntry);
            else {
                // turn off INT0 interrupt
                EIMSK &= ~(_BV(INT0));
                spi_post_adc_noise();
                change_state(MCURunningEntry);
            }
            break;
/*--------------------------------------------------------*/
        case SignaledOffEntry:
#ifdef USE_LED
            LED4_SET_ON;
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
#endif
            SHUTDOWN_SET_ON;
            change_state(SignaledOff);
            break;
        case SignaledOff:
            if (!mcu_is_running())
            {
                SHUTDOWN_SET_OFF;
                change_state(MCUOffEntry);
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
            change_state(PowerDownEntry);
            break;
/*--------------------------------------------------------*/
        case PowerDownEntry:
#ifdef USE_LED
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;
            LED7_SET_OFF;
#endif
            // enter Power-Down mode
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);

            // SHUTDOWN pin to input and pull-up on
            SHUTDOWN_DIR &= ~(_BV(SHUTDOWN));
            SHUTDOWN_PORT |= _BV(SHUTDOWN);

            // modules power off
            sensor_pre_power_down();
            spi_pre_power_down();
            
            // set INTO interrupt
            EIMSK |= _BV(INT0);

            // do the power down, if no INT0 interrupt
            cli();
            if (!int0_event) {
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
            }
            sei();
            change_state(PowerDownExit);
            // NO BREAK, fall through to PowerDownExit
/*--------------------------------------------------------*/
        case PowerDownExit:
            // turn off INT0 interrupt
            EIMSK &= ~(_BV(INT0));
            
            spi_post_power_down();
            sensor_post_power_down();
            // SHUTDOWN pin pull up off, to output
            SHUTDOWN_PORT &= ~(_BV(SHUTDOWN));
            SHUTDOWN_DIR |= _BV(SHUTDOWN);
            wakeup_timer = 0;
            change_state(WaitEntry);
            break;
/*--------------------------------------------------------*/
        default:
            change_state(Start);
        }
/*--------------------------------------------------------*/
        spi_state_machine();
        sensor_state_machine();

        int0_event = 0;
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
    if (button_timer >= 0)
        button_timer++;
    if (wakeup_timer >= 0)
        wakeup_timer++;
    if (idle_timer >= 0)
        idle_timer++;
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
    int0_event = 1;
}
