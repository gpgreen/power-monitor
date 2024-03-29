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
 *  Boot Flash section=2048 words Boot start address=$3800 [BOOTSZ=00]
 *  Enable Serial Program [SPIEN=0]
 *  Brown-out at VCC=2.7
 *  Low=0xe2 Hi=0xd9 Ext=0xFd
 *  from http://www.engbedded.com/fusecalc/
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

/*-----------------------------------------------------------------------*/

// simavr
#include <avr/avr_mcu_section.h>
AVR_MCU(F_CPU, "atmega328p");
AVR_MCU_VOLTAGES(3300, 3300, 0) /* VCC, AVCC, VREF - millivolts */

/*--------------------------------------------------------
 * globals
 *--------------------------------------------------------*/

// this will be non-zero if this is hat with CAN hardware
int g_can_hardware;

/*--------------------------------------------------------
 * static locals
 *--------------------------------------------------------*/

StateMachine machine_state;
StateMachine prev_state;

// button state mask updated by timer interrupt
// will be 0xFF when button up, 0x00 when down (debounced)
volatile uint8_t button_mask;

// mcu_running state mask updated by timer interrupt
// will be 0xFF when mcu_running up, 0x00 when down (debounced)
volatile uint8_t mcu_running_mask;

// number of timer interrupt while button is down
volatile int8_t button_timer;

// number of timer interrupts for wake up period
volatile int8_t wakeup_timer;

// flag for wakeup event
volatile uint8_t we_event;

/*--------------------------------------------------------*/

void set_enable(int direction)
{
    if (direction) {
        ENABLE_PORT |= _BV(ENABLE);
    } else {
        ENABLE_PORT &= ~_BV(ENABLE);
    }
}

/*--------------------------------------------------------*/

// setup pins before/after power down if they have no pull up/down
void sleep_output_pins(int pre_sleep)
{
    if (pre_sleep) {
        LED1_DIR &= ~_BV(LED1);
        LED1_PORT |= _BV(LED1);

        SHUTDOWN_DIR &= ~_BV(SHUTDOWN);
        SHUTDOWN_PORT |= _BV(SHUTDOWN);

    } else {
        LED1_PORT &= ~_BV(LED1);
        LED1_DIR |= _BV(LED1);
        // LED
        LED1_SET_OFF;

        // SHUTDOWN, output, no pullup
        SHUTDOWN_PORT &= ~_BV(SHUTDOWN);
        SHUTDOWN_DIR |= _BV(SHUTDOWN);
        // shutdown inactive is low
        SHUTDOWN_SET_OFF;
    }
}

/*--------------------------------------------------------*/

void
init(void)
{
    // turn off modules that aren't used
    PRR |= (_BV(PRTWI)|_BV(PRTIM2)|_BV(PRTIM1)|_BV(PRUSART0));

    // timer0 set to CK/256, overflow interrupt enabled
    TCCR0B = _BV(CS02);
    TIMSK0 = _BV(TOIE0);

    // ENABLE, output, no pullup (external pull-down)
    ENABLE_DIR  |= _BV(ENABLE);

    // enable inactive is low
    set_enable(0);

    // MCU_RUNNING, input, no pullup (external pull-down)
    MCU_RUNNING_DIR &= ~(_BV(MCU_RUNNING));

    // BUTTON, input, no pullup (external pull-up)
    BUTTON_DIR &= ~(_BV(BUTTON));

    // hardware indication, input w/pullup
    HDWR_ID_DIR &= ~_BV(HDWR_ID);
    HDWR_ID_PORT |= _BV(HDWR_ID);
    // if hardware ident is low, this has CAN
    if (!(HDWR_ID_PIN & _BV(HDWR_ID))) {
        g_can_hardware = 1;
        // disable pullup on grounded pin
        HDWR_ID_PORT &= ~_BV(HDWR_ID);
    }

    // set pins
    sleep_output_pins(0);

#ifdef USE_LED
    // PORTD setup PINS for output
    DDRD |= (_BV(LED2)|_BV(LED3)|_BV(LED4));
    // PORTC setup PINS for output
    DDRC |= (_BV(LED6)|_BV(LED5));
#else
    // set unused ports as input and pull-up on
    DDRD &= ~(_BV(0)|_BV(1)|_BV(5)|_BV(6)|_BV(7));
    PORTD |= (_BV(0)|_BV(1)|_BV(5)|_BV(6)|_BV(7));
#endif

    // the other components
    sensor_init();
    spi_init();
}

/* -- Transition Functions -- */

/*--------------------------------------------------------*/
// trigger when MCU activates MCU_RUNNING signal
int mcu_is_running(void)
{
    return mcu_running_mask == 0xFF;
}

/*--------------------------------------------------------*/
// trigger when button pressed
int button_pressed(void)
{
    return button_mask == 0x00;
}

/*--------------------------------------------------------*/
// trigger when button released
int button_released(void)
{
    return button_mask == 0xFF;
}

/*--------------------------------------------------------*/
// trigger when the wake up period is over
int wake_up_expired(void)
{
    // this is 750ms
    if (wakeup_timer >= (uint8_t)(F_CPU/256/256/1.5))
        return 1;
    return 0;
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
        case SignaledOffEntry:
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
    CLKPR = _BV(CLKPCE);
    CLKPR = 0;

    init();

    machine_state = prev_state = Start;
    button_mask = 0xFF;
    wakeup_timer = -1;
    button_timer = -1;

    // start watchdog
    wdt_enable(WDTO_250MS);

	// start interrupts
	sei();

    wakeup_timer = 0;

    // main loop
    while(1)
    {
        wdt_reset();
/*--------------------------------------------------------*/
        switch (machine_state) {
        case Start:
            if (wake_up_expired())
                change_state(WaitEntry);
            break;
/*--------------------------------------------------------*/
        case WaitEntry:
#ifdef USE_LED
            LED2_SET_ON;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
#endif
            wakeup_timer = 0;
            set_enable(0);
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
            LED3_SET_ON;
            LED2_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
#endif
            wakeup_timer = -1;
            set_enable(1);
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
            LED4_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED5_SET_OFF;
#endif
            change_state(MCURunning);
            break;
        case MCURunning:
            if (button_pressed())
            {
                change_state(ButtonPress);
            }
            // if turned off via the desktop
            if (!mcu_is_running())
            {
                change_state(MCUOffEntry);
            }
            break;
/*--------------------------------------------------------*/
        case SignaledOffEntry:
#ifdef USE_LED
            LED5_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
#endif
            LED1_SET_ON;
            SHUTDOWN_SET_ON;
            change_state(SignaledOff);
            break;
        case SignaledOff:
            if (!mcu_is_running())
            {
                LED1_SET_OFF;
                SHUTDOWN_SET_OFF;
                change_state(MCUOffEntry);
            }
            break;
/*--------------------------------------------------------*/
        case MCUOffEntry: case MCUOff:
#ifdef USE_LED
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
#endif
            set_enable(0);
            change_state(PowerDown);
            break;
/*--------------------------------------------------------*/
        case PowerDown:
#ifdef USE_LED
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;
#endif
            // enter Power-Down mode
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);

            // turn off Timer0 ovf interrupt
            TIMSK0 &= ~_BV(TOIE0);

            sleep_output_pins(1);

            // modules power off
            sensor_pre_power_down();
            spi_pre_power_down();
            // turn off Timer0
            PRR |= _BV(PRTIM0);

            // set INTO interrupt (Button/PD2)
            EIMSK |= _BV(INT0);

            // disable wdt
            wdt_disable();

            // do the power down, if no interrupt's
            cli();
            if (!we_event) {
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
            }
            sei();

            // enable the watchdog
            wdt_enable(WDTO_250MS);

            // turn off INT0 interrupt (Button/PD2)
            EIMSK &= ~(_BV(INT0));

            // turn on modules
            PRR &= ~_BV(PRTIM0);
            spi_post_power_down();
            sensor_post_power_down();

            sleep_output_pins(0);

            // set Timer0 ovf interrupt
            TIMSK0 |= _BV(TOIE0);

            change_state(WaitEntry);
            break;
/*--------------------------------------------------------*/
        default:
            change_state(Start);
        }
/*--------------------------------------------------------*/
        spi_state_machine();
        sensor_state_machine();

        we_event = 0;
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
    mcu_running_mask <<= 1;
    if (MCU_RUNNING_ON)
        mcu_running_mask |= 1;
    else
        mcu_running_mask &= ~1;
    if (button_timer >= 0)
        button_timer++;
    if (wakeup_timer >= 0)
        wakeup_timer++;
#ifdef USE_LED
//    TOGGLE_LED6;
#endif
}

/*
 * INT0 interrupt
 * interrupt flag cleared by hardware
 */
ISR(INT0_vect)
{
    we_event = 1;
}
