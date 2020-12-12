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
 * ATtiny12
 * signature = 0x1e9005
 * Fuse bits for ATtiny12
 *  Int RC osc 1.2MHz; Start-up time 6CK+67ms;[CKSEL=0010];
 *  Enable Serial Program [SPIEN=0]
 *  Brown-out detection at VCC=2.3V; [BODLEVEL=0]
 *  fuse=0x52 [0101 0011]
 *  from http://www.engbedded.com/fusecalc/
 * This is the default fuse settings, so leave it alone
 *
 * to set fuses:
 * avrdude -c usbtiny -p attiny12 -U fuse:w:0x52:m
 * to read fuses:
 * avrdude -c usbtiny -p attiny12 -U fuse:r:fuse.txt:h
 *
 * PINOUT
 * ------
 *            +----------+
 *            | ATtiny12 |
 *      RESET-|1        8|-VCC
 *   SHUTDOWN-|2        7|-ENABLE       [SCK]
 *           -|3        6|-BUTTON       [MISO]
 *        GND-|4        5|-MCU_RUNNING  [MOSI]
 *            |          |    
 *            +----------+     
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// Define pin labels
// these are all portB
#define MCU_RUNNING 0
#define BUTTON 1
#define ENABLE 2
#define SHUTDOWN 3
#define RESET 5

typedef enum {
    Start,
    WaitSignalOn,
    SignaledOn,
    MCURunning,
    SignaledOff,
    MCUOff
} StateMachine;

StateMachine machine_state;

uint8_t buttonpress;
uint8_t button_state;
volatile uint8_t button_mask;
volatile uint8_t tovflows;

void
ioinit(void)
{
    // set pullups on unused pins
    // PORTA setup pins for output

    // setup PINS for input, RESET is already set as input and pull up on
    // due to fuse setting
    DDRB &= ~(_BV(BUTTON)|_BV(MCU_RUNNING));
    // set pullups on input pins
    //PORTB |= _BV(DI)|_BV(SCK);

    // PORTB setup PINS for output
    DDRB |= _BV(ENABLE)|_BV(SHUTDOWN);
    // enable is pulled low
    PORTB &= ~(_BV(ENABLE));
    // shutdown is pulled low
    PORTB &= ~(_BV(SHUTDOWN));
    
    // timer set to CK/1024, overflow interrupt enabled
    TCCR0B = _BV(CS02)|_BV(CS00);
    TIMSK0 = _BV(TOIE0);
    
    // set BUTTON pin change interrupt
    //MCUCR |= _BV(ISC00);
    //GIMSK |= _BV(INT0);

    machine_state = Start;
    buttonpress = 0;
    button_state = 0;
    button_mask = 0xFF;
}

inline
int mcu_is_running(void)
{
    return (PINB & _BV(MCU_RUNNING));
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
    ioinit();

	// start interrupts
	sei();

    // main loop
    while(1)
    {
        // check if button down
        if (button_state == 0 && button_mask == 0x00) {
            button_state = 1;
            tovflows = 0;
            // has it been down for long enough
        } else if (button_state == 1) {
            // released too early
            if (button_mask != 0x00)
                button_state = 0;
            // is it long enough
            else if (button_mask == 0x00 && tovflows >= 10) {
                button_state = 2;
            }
            // down long enough, check for release
        } else if (button_state == 2 && button_mask == 0xFF) {
            button_state = 0;
            buttonpress = 1;
        }
        
        switch (machine_state) {
        case Start:
            PORTB &= ~(_BV(ENABLE)|_BV(SHUTDOWN));
            machine_state = WaitSignalOn;
            break;
        case WaitSignalOn:
            if (button_pressed())
                machine_state = SignaledOn;
            break;
        case SignaledOn:
            PORTB |= _BV(ENABLE);
            if (mcu_is_running())
                machine_state = MCURunning;
            if (button_pressed())
                machine_state = MCUOff;
            break;
        case MCURunning:
            if (button_pressed())
                machine_state = SignaledOff;
            break;
        case SignaledOff:
            PORTB |= _BV(SHUTDOWN);
            if (!mcu_is_running())
                machine_state = MCUOff;
            break;
        case MCUOff:
            PORTB &= ~(_BV(ENABLE)|_BV(SHUTDOWN));
            machine_state = WaitSignalOn;
            break;
        default:
            machine_state = Start;
        }
    }
    return 0;
}

/*
 * Timer0 overflow interrupt
 * interrupt flag cleared by hardware
 */
ISR(TIM0_OVF_vect)
{
    static uint8_t count = 0;
    // about 32ms
    if (++count == F_CPU / 1024 / 32)
    {
        button_mask <<= 1;
        button_mask |= ((PINB & _BV(BUTTON)) == _BV(BUTTON));
        count = 0;
    }
    if (button_state == 1)
        tovflows++;
}

/*
 * INT0 interrupt
 * interrupt flag cleared by hardware
 */
ISR(INT0_vect)
{
}
