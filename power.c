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
 * signature = 0x1e9109
 * Fuse bits for ATtiny26
 *  Int RC osc 1.0MHz; Start-up time 6CK+65ms;[CKSEL=0001 SUT=10];
 *  Enable Serial Program [SPIEN=0]
 *  Brown-out detection at VCC=4.0V; [BODLEVEL=0]
 *  Low=0xe1 [11100001] Hi=0xf5 [11110101]
 *  from http://www.engbedded.com/fusecalc/
 *
 * to set fuses:
 * avrdude -c usbtiny -p attiny26 -U lfuse:w:0xe1:m -U hfuse:w:0xf5:m
 * to read fuses:
 * avrdude -c usbtiny -p attiny26 -U lfuse:r:lfuse.txt:h -U hfuse:r:hfuse.txt:h
 *
 * PINOUT
 * ------
 *            +----------+
 *            | ATtiny26 |
 *       MOSI-|1       20|-
 *       MISO-|2       19|-
 *        SCK-|3       18|-LED6
 *   SHUTDOWN-|4       17|-LED5
 *        VCC-|5       16|-GND
 *        GND-|6       15|-AVCC
 *MCU_RUNNING-|7       14|-LED4
 *     ENABLE-|8       13|-LED3
 *     BUTTON-|9       12|-LED2
 *      RESET-|10      11|-LED1
 *            |          |
 *            +----------+
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// Define pin labels
// these are all portB
#define MCU_RUNNING 4
#define BUTTON 6
#define ENABLE 5
#define SHUTDOWN 3
#define RESET 7

// these are all portA
#define LED1 7
#define LED2 6
#define LED3 5
#define LED4 4
#define LED5 3
#define LED6 2

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
    // PORTA setup PINS for output
    DDRA |= (_BV(LED1)|_BV(LED2)|_BV(LED3)|_BV(LED4)|_BV(LED5)|_BV(LED6));
    
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
    
    // timer set to CK/8, overflow interrupt enabled
    TCCR0 = _BV(CS01);
    TIMSK = _BV(TOIE0);
    
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
        PORTA &= ~(_BV(LED1));
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
            PORTA |= _BV(LED1);
        }
        
        switch (machine_state) {
        case Start:
            PORTA &= ~(_BV(LED2)|_BV(LED3)|_BV(LED4)|_BV(LED5)|_BV(LED6));

            PORTB &= ~(_BV(ENABLE)|_BV(SHUTDOWN));
            machine_state = WaitSignalOn;
            break;
        case WaitSignalOn:
            PORTA |= _BV(LED2);
            PORTA &= ~(_BV(LED3)|_BV(LED4)|_BV(LED5)|_BV(LED6));

            if (button_pressed())
                machine_state = SignaledOn;
            break;
        case SignaledOn:
            PORTA |= _BV(LED3);
            PORTA &= ~(_BV(LED2)|_BV(LED4)|_BV(LED5)|_BV(LED6));

            PORTB |= _BV(ENABLE);
            if (mcu_is_running())
                machine_state = MCURunning;
            if (button_pressed())
                machine_state = MCUOff;
            break;
        case MCURunning:
            PORTA |= _BV(LED4);
            PORTA &= ~(_BV(LED2)|_BV(LED3)|_BV(LED5)|_BV(LED6));

            if (button_pressed())
                machine_state = SignaledOff;
            break;
        case SignaledOff:
            PORTA |= _BV(LED5);
            PORTA &= ~(_BV(LED2)|_BV(LED3)|_BV(LED4)|_BV(LED6));

            PORTB |= _BV(SHUTDOWN);
            if (!mcu_is_running())
                machine_state = MCUOff;
            break;
        case MCUOff:
            PORTA |= _BV(LED6);
            PORTA &= ~(_BV(LED2)|_BV(LED3)|_BV(LED4)|_BV(LED5));

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
ISR(TIMER0_OVF0_vect)
{
    button_mask <<= 1;
    if (PINB & _BV(BUTTON))
        button_mask |= 1;
    else
        button_mask &= ~1;
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
