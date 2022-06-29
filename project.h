/*
 * project.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * Hardware Changes
 * ----------------
 * Rev C
 * Added HDWR_ID
 * board has CAN hardware connected to Pi header
 * pins for led added to connector
 *
 * Rev B
 * LED1 attached to PB0
 * LED removed from SHUTDOWN
 * SHUTDOWN added pulldown
 * MCU_RUNNING moved from PD3 to PB1
 * MCU_RUNNING added pulldown
 * LC circuit added to AVCC
 * Optional 12V ADC measurement to ADC7
 *
 * Rev A
 * Original
 *
 * PINOUTS
 * -------
 * See [README.md] for pinouts
 *
 */
#ifndef PROJECT_H_
#define PROJECT_H_

/*----------------------------------------------------------------*/
// global variables
/*----------------------------------------------------------------*/

extern int g_can_hardware;

/*----------------------------------------------------------------*/

// version of firmware
#define MAJOR_VERSION   0
#define MINOR_VERSION   4

// MCU part, the Chart Plotter Hat uses the 32pin version
#ifndef USE_28PIN
#ifndef USE_32PIN
#error "Either -DUSE_28PIN or -DUSE_32PIN must be used"
#endif
#endif

// Define whether to use LED's for debugging or not
#ifdef USE_28PIN
 #define USE_LED 1
#endif

/*----------------------------------------------------------------*/
// Define pin labels

// PortD
#define MCU_RUNNING_PORT PORTB
#define MCU_RUNNING_PIN PINB
#define MCU_RUNNING_DIR DDRB
#define MCU_RUNNING 1

#define BUTTON_PORT PORTD
#define BUTTON_PIN PIND
#define BUTTON_DIR DDRD
#define BUTTON 2

#ifdef USE_32PIN
#define HDWR_ID_PORT PORTD
#define HDWR_ID_PIN PIND
#define HDWR_ID_DIR DDRD
#define HDWR_ID 3
#endif

#define ENABLE_PORT PORTD
#define ENABLE_PIN PIND
#define ENABLE_DIR DDRD
#define ENABLE 4

#ifdef USE_28PIN
#define SHUTDOWN_PORT PORTD
#define SHUTDOWN_PIN PIND
#define SHUTDOWN_DIR DDRD
#define SHUTDOWN 3
#endif

// PortC
#define RESET 6

#ifdef USE_28PIN
#define HDWR_ID_PORT PORTC
#define HDWR_ID_PIN PINC
#define HDWR_ID_DIR DDRC
#define HDWR_ID 5
#endif

// PortB
#ifdef USE_32PIN
#define SHUTDOWN_PORT PORTB
#define SHUTDOWN_PIN PINB
#define SHUTDOWN_DIR DDRB
#define SHUTDOWN 6
#endif

#define EEPROM_PORT PORTB
#define EEPROM_PIN PINB
#define EEPROM_DIR DDRB
#define EEPROM 7

#define SPI_PORT PORTB
#define SPI_DIR DDRB
#define MISO 4
#define MOSI 3
#define SCK 5

#define CS_PORT PORTB
#define CS_PIN PINB
#define CS_DIR DDRB
#define CS 2

#define LED1_PORT PORTB
#define LED1_DIR DDRB
#define LED1 0

#define LED1_SET_ON PORTB |= _BV(LED1)
#define LED1_SET_OFF PORTB &= ~(_BV(LED1))
#define TOGGLE_LED1 \
    {if (bit_is_set(PORTB, LED1)) PORTB&=~(_BV(LED1)); else PORTB|=_BV(LED1);}

/*----------------------------------------------------------------*/
// Define pin macros

#define MCU_RUNNING_ON bit_is_set(MCU_RUNNING_PIN, MCU_RUNNING)
#define MCU_RUNNING_OFF bit_is_clear(MCU_RUNNING_PIN, MCU_RUNNING)

#define CS_ON bit_is_set(CS_PIN, CS)
#define CS_OFF bit_is_clear(CS_PIN, CS)

#define BUTTON_ON bit_is_set(BUTTON_PIN, BUTTON)
#define BUTTON_OFF bit_is_clear(BUTTON_PIN, BUTTON)

#define SHUTDOWN_SET_ON SHUTDOWN_PORT |= _BV(SHUTDOWN)
#define SHUTDOWN_SET_OFF SHUTDOWN_PORT &= ~(_BV(SHUTDOWN))

/*----------------------------------------------------------------
 * USE_LED is set if we hav LED's set on unused ports for
 * debugging power state
 -----------------------------------------------------------------*/
#ifdef USE_LED

// PortD LED's
#define LED2 7
#define LED3 6
#define LED4 5
// PortB LED's
#define LED1 0
// PortC LED's
#define LED5 3
#define LED6 4

#define TOGGLE_LED6 \
    {if (bit_is_set(PORTC, LED6)) PORTC&=~(_BV(LED6)); else PORTC|=_BV(LED6);}

#define LED2_SET_ON PORTD |= _BV(LED2)
#define LED3_SET_ON PORTD |= _BV(LED3)
#define LED4_SET_ON PORTD |= _BV(LED4)
#define LED5_SET_ON PORTC |= _BV(LED5)
#define LED6_SET_ON PORTC |= _BV(LED6)

#define LED2_SET_OFF PORTD &= ~(_BV(LED2))
#define LED3_SET_OFF PORTD &= ~(_BV(LED3))
#define LED4_SET_OFF PORTD &= ~(_BV(LED4))
#define LED5_SET_OFF PORTC &= ~(_BV(LED5))
#define LED6_SET_OFF PORTC &= ~(_BV(LED6))

#endif

#endif /* PROJECT_H_ */
