/*
 * power-monitor
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * This device will monitor a momentary pushbutton. If the button
 * is pushed, the device will enable a switching power supply.
 * This will turn on the Raspberry Pi. Once it is running, it will
 * pull a pin high and hold it there to show it is running. If the
 * button is pushed again, this device will pull a pin high that is monitored
 * by the Pi to signal a shutdown. Once the Pi has powered down
 * the pin that it has been holding high will drop. This
 * allows this device to power down the switching power supply.
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
 * to set fuses:
 * avrdude -c usbtiny -p atmega328p -U lfuse:w:0xc2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
 * to read fuses:
 * avrdude -c usbtiny -p atmega328p -U lfuse:r:lfuse.txt:h -U hfuse:r:hfuse.txt:h -U efuse:r:efuse.txt:h
 *
 * PINOUT
 * ------
 *             +--------------------+
 *             |                    |
 *       RESET-|1  PC6        PC5 29|-
 *            -|2  PD0        PC4 27|-
 *            -|3  PD1        PC3 26|-    
 *      BUTTON-|4  PD2        PC2 25|-    
 * MCU_RUNNING-|5  PD3        PC1 24|-   
 *      ENABLE-|6  PD4        PC0 23|-    
 *         VCC-|7                 22|-GND 
 *         GND-|8                 21|-AREF
 *    SHUTDOWN-|9  PB6            20|-AVCC
 *        LED1-|10 PB7        PB5 19|-SCK
 *        LED2-|11 PD5        PB4 18|-MISO
 *        LED3-|12 PD6        PB3 17|-MOSI
 *        LED4-|13 PD7        PB2 16|-SS
 *        LED5-|14 PB0        PB1 15|-LED6
 *             |                    |
 *             +--------------------+
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "sensor.h"

// Define pin labels
// these are all portD
#define MCU_RUNNING 3
#define BUTTON 2
#define ENABLE 4
#define LED2 5
#define LED3 6
#define LED4 7

// these are all portC
#define RESET 6

// these are all portB
#define SHUTDOWN 6
#define LED1 7
#define LED5 0
#define LED6 1

///////////////////////////////////////////////////////////////////

#define MCU_RUNNING_ON bit_is_set(PIND, MCU_RUNNING)
#define MCU_RUNNING_OFF bit_is_clear(PIND, MCU_RUNNING)

#define BUTTON_ON bit_is_set(PIND, BUTTON)
#define BUTTON_OFF bit_is_clear(PIND, BUTTON)

#define SHUTDOWN_SET_ON PORTB |= _BV(SHUTDOWN)
#define SHUTDOWN_SET_OFF PORTB &= ~(_BV(SHUTDOWN))

#define ENABLE_SET_ON PORTD |= _BV(ENABLE)
#define ENABLE_SET_OFF PORTD &= ~(_BV(ENABLE))

#define TOGGLE_LED1 \
    {if (bit_is_set(PORTB, LED1)) PORTB&=~(_BV(LED1)); else PORTB|=_BV(LED1);}
    
#define LED1_SET_ON PORTB |= _BV(LED1)
#define LED2_SET_ON PORTD |= _BV(LED2)
#define LED3_SET_ON PORTD |= _BV(LED3)
#define LED4_SET_ON PORTD |= _BV(LED4)
#define LED5_SET_ON PORTB |= _BV(LED5)
#define LED6_SET_ON PORTB |= _BV(LED6)

#define LED1_SET_OFF PORTB &= ~(_BV(LED1))
#define LED2_SET_OFF PORTD &= ~(_BV(LED2))
#define LED3_SET_OFF PORTD &= ~(_BV(LED3))
#define LED4_SET_OFF PORTD &= ~(_BV(LED4))
#define LED5_SET_OFF PORTB &= ~(_BV(LED5))
#define LED6_SET_OFF PORTB &= ~(_BV(LED6))

///////////////////////////////////////////////////////////////

    typedef enum {
        Start,
        WaitSignalOn,
        SignaledOn,
        MCURunning,
        SignaledOff,
        MCUOff,
        LowPowerMode
    } StateMachine;

StateMachine machine_state;

//////////////////////////////////////////////////////////////
// globals
//////////////////////////////////////////////////////////////

// 1 when buttonpress detected, 0 otherwise
uint8_t buttonpress;
// 0 = up, 1 = down detected, waiting for delay
uint8_t button_state;
// button state mask updated by timer interrupt
// will be 0xFF when button up, 0x00 when down (debounced)
volatile uint8_t button_mask;
// number of timer interrupt while button is down
volatile uint8_t tovflows;

//////////////////////////////////////////////////////////////

void
init(void)
{
    // set pullups on unused pins
    // PORTD setup PINS for output
    DDRD |= (_BV(ENABLE)|_BV(LED2)|_BV(LED3)|_BV(LED4));

    // PORTB setup PINS for output
    DDRB |= (_BV(SHUTDOWN)|_BV(LED1)|_BV(LED5)|_BV(LED6));
    
    // setup PINS for input, RESET is already set as input and pull up on
    // due to fuse setting
    DDRD &= ~(_BV(BUTTON)|_BV(MCU_RUNNING));

    // enable is pulled low
    ENABLE_SET_OFF;

    // shutdown is pulled low
    SHUTDOWN_SET_OFF;
    
    // timer set to CK/8, overflow interrupt enabled
    TCCR0B = _BV(CS01);
    TIMSK0 = _BV(TOIE0);
}

inline
int mcu_is_running(void)
{
    return (bit_is_set(PIND,MCU_RUNNING));
}

inline
int button_pressed(void)
{
    if (buttonpress) {
        buttonpress = 0;
        return 1;
    }
    return 0;
}

int
main(void)
{
    // set the clock to 8MHz
//    CLKPR = _BV(CLKPCE);
//    CLKPR = 0;

    // turn off modules that aren't used
    PRR |= (_BV(PRTWI)|_BV(PRTIM2)|_BV(PRTIM1)|_BV(PRUSART0));
    
    init();
    sensor_init();
    
    machine_state = Start;
    button_mask = 0xFF;
    
	// start interrupts
	sei();

    // main loop
    while(1)
    {
        // check if button down, mask length is 2.048ms
        if (button_state == 0 && button_mask == 0x00) {
            button_state = 1;
            tovflows = 0;
            // has it been down for long enough
        } else if (button_state == 1) {
            if (button_mask == 0x0FF) {
                // delay long enough, each overflow = 256us, this is 25.6ms
                if (tovflows >= F_CPU / 8 / 256 / 100) {
                    buttonpress = 1;
                }
                button_state = 0;
            }
        }
        
        switch (machine_state) {
        case Start:
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;

            ENABLE_SET_OFF;
            SHUTDOWN_SET_OFF;

            machine_state = WaitSignalOn;
            break;
        case WaitSignalOn:
            LED2_SET_ON;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;

            if (button_pressed())
                machine_state = SignaledOn;
            break;
        case SignaledOn:
            LED3_SET_ON;
            LED2_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;

            ENABLE_SET_ON;

            if (mcu_is_running())
                machine_state = MCURunning;
            if (button_pressed())
                machine_state = MCUOff;
            break;
        case MCURunning:
            LED4_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;

            if (button_pressed())
                machine_state = SignaledOff;
            break;
        case SignaledOff:
            LED5_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED6_SET_OFF;

            SHUTDOWN_SET_ON;

            if (!mcu_is_running())
                machine_state = MCUOff;
            break;
        case MCUOff:
            LED6_SET_ON;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;

            ENABLE_SET_OFF;
            SHUTDOWN_SET_OFF;

            machine_state = LowPowerMode;
            break;
        case LowPowerMode:
            LED1_SET_OFF;
            LED2_SET_OFF;
            LED3_SET_OFF;
            LED4_SET_OFF;
            LED5_SET_OFF;
            LED6_SET_OFF;

            // enter Power-Down mode
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            // set INTO interrupt
            EIMSK |= _BV(INT0);
            sensor_pre_power_down();
            sleep_enable();
            sleep_mode();

            // woken up
            // turn off INT0 interrupt
            EIMSK &= ~(_BV(INT0));
            sensor_post_power_down();
            machine_state = WaitSignalOn;
            break;
        default:
            machine_state = Start;
        }

        sensor_state_machine();
    }
    return 0;
}

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

    TOGGLE_LED1;
}

/*
 * INT0 interrupt
 * interrupt flag cleared by hardware
 */
ISR(INT0_vect)
{
    // does nothing but wake up the cpu
}
