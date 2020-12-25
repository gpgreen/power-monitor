/*
 * project.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * PINOUT
 * ------            ATmega328P
 *             +--------------------+
 *             |                    |
 *       RESET-|1  PC6        PC5 29|-ADC5
 *            -|2  PD0        PC4 27|-ADC4
 *            -|3  PD1        PC3 26|-ADC3
 *      BUTTON-|4  PD2        PC2 25|-ADC2
 * MCU_RUNNING-|5  PD3        PC1 24|-ADC1
 *      ENABLE-|6  PD4        PC0 23|-ADC0
 *         VCC-|7                 22|-GND 
 *         GND-|8                 21|-AREF
 *    SHUTDOWN-|9  PB6            20|-AVCC
 *        LED1-|10 PB7        PB5 19|-SCK
 *        LED2-|11 PD5        PB4 18|-MISO
 *        LED3-|12 PD6        PB3 17|-MOSI
 *        LED4-|13 PD7        PB2 16|-CS
 *        LED5-|14 PB0        PB1 15|-LED6
 *             |                    |
 *             +--------------------+
 */
#ifndef PROJECT_H_
#define PROJECT_H_

/*----------------------------------------------------------------*/
// Define pin labels

// PortD
#define MCU_RUNNING 3
#define BUTTON 2
#define ENABLE 4
#define LED2 5
#define LED3 6
#define LED4 7

// PortC
#define RESET 6

// PortB
#define SHUTDOWN 6
#define LED1 7
#define LED5 0
#define LED6 1
#define MISO 4
#define MOSI 3
#define SCK 5
#define CS 2

/*----------------------------------------------------------------*/
// Define pin macros

#define MCU_RUNNING_ON bit_is_set(PIND, MCU_RUNNING)
#define MCU_RUNNING_OFF bit_is_clear(PIND, MCU_RUNNING)

#define CS_ON bit_is_set(PINB, CS)
#define CS_OFF bit_is_clear(PINB, CS)

#define BUTTON_ON bit_is_set(PIND, BUTTON)
#define BUTTON_OFF bit_is_clear(PIND, BUTTON)

#define SHUTDOWN_SET_ON PORTB |= _BV(SHUTDOWN)
#define SHUTDOWN_SET_OFF PORTB &= ~(_BV(SHUTDOWN))

#define ENABLE_SET_ON PORTD |= _BV(ENABLE)
#define ENABLE_SET_OFF PORTD &= ~(_BV(ENABLE))

#define TOGGLE_LED1 \
    {if (bit_is_set(PORTB, LED1)) PORTB&=~(_BV(LED1)); else PORTB|=_BV(LED1);}
#define TOGGLE_LED5 \
    {if (bit_is_set(PORTB, LED5)) PORTB&=~(_BV(LED5)); else PORTB|=_BV(LED5);}
#define TOGGLE_LED6 \
    {if (bit_is_set(PORTB, LED6)) PORTB&=~(_BV(LED6)); else PORTB|=_BV(LED6);}
    
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

#endif /* PROJECT_H_ */
