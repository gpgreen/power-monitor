/*
 * project.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * Hardware Changes
 * ----------------
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
 *
 *                 ATmega328P-28P3            
 *             +--------------------+         
 *             |                    |         
 *       RESET-|1  PC6        PC5 29|-ADC5    
 *        LED7-|2  PD0        PC4 27|-ADC4    
 *        LED6-|3  PD1        PC3 26|-ADC3    
 *      BUTTON-|4  PD2        PC2 25|-ADC2    
 *            -|5  PD3        PC1 24|-ADC1    
 *      ENABLE-|6  PD4        PC0 23|-ADC0    
 *         VCC-|7                 22|-GND     
 *         GND-|8                 21|-AREF    
 *    SHUTDOWN-|9  PB6            20|-AVCC    
 *        LED5-|10 PB7        PB5 19|-SCK     
 *        LED4-|11 PD5        PB4 18|-MISO    
 *        LED3-|12 PD6        PB3 17|-MOSI    
 *        LED2-|13 PD7        PB2 16|-CS
 *        LED1-|14 PB0        PB1 15|-MCU_RUNNING
 *             |                    |         
 *             +--------------------+         
 *          
 *                    ATmega328P-32A
 *             +--------------------------+        
 *             |                          |        
 *            -|4  VCC              PB0 12|-LED1   
 *            -|18 AVCC             PB1 13|-MCU_RUNNING
 *            -|20 AREF             PB2 14|-CS
 *            -|3  GND              PB3 15|-MOSI   
 *             |                    PB4 16|-MISO   
 *          A6-|19 ADC6             PB5 17|-SCK    
 *          A7-|22 ADC7             PB6  7|-SHUTDOWN
 *             |                    PB7  8|-EEPROM 
 *            -|30 PD0                    |        
 *            -|31 PD1              PC0 23|-A0     
 *      BUTTON-|32 PD2              PC1 24|-A1     
 *            -|1  PD3              PC2 25|-A2     
 *          EN-|2  PD4              PC3 26|-A3     
 *            -|9  PD5              PC4 27|-A4     
 *            -|10 PD6              PC5 28|-A5     
 *            -|11 PD7              PC6 29|-RESET  
 *             |                          |        
 *             +--------------------------+        
 *
 */
#ifndef PROJECT_H_
#define PROJECT_H_

// version of firmware
#define MAJOR_VERSION   0
#define MINOR_VERSION   2

// MCU part, the Chart Plotter Hat uses the 32pin version
//#define USE_28PIN 1
#define USE_32PIN 1

// Define whether to use LED's for debugging or not
#define USE_LED 1

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

#define ENABLE_PORT PORTD
#define ENABLE_PIN PIND
#define ENABLE_DIR DDRD
#define ENABLE 4

// PortC
#define RESET 6

// PortB
#define SHUTDOWN_PORT PORTB
#define SHUTDOWN_PIN PINB
#define SHUTDOWN_DIR DDRB
#define SHUTDOWN 6

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

#define ENABLE_SET_ON ENABLE_PORT |= _BV(ENABLE)
#define ENABLE_SET_OFF ENABLE_PORT &= ~(_BV(ENABLE))

/*----------------------------------------------------------------
 * USE_LED is set if we hav LED's set on unused ports for
 * debugging power state
 -----------------------------------------------------------------*/
#ifdef USE_LED

// PortD LED's
#define LED2 7
#define LED3 6
#define LED4 5
#define LED6 1
#define LED7 0
// PortB LED's
#define LED1 0
#define LED5 7

#define TOGGLE_LED1 \
    {if (bit_is_set(PORTB, LED1)) PORTB&=~(_BV(LED1)); else PORTB|=_BV(LED1);}
#define TOGGLE_LED5 \
    {if (bit_is_set(PORTB, LED5)) PORTB&=~(_BV(LED5)); else PORTB|=_BV(LED5);}
#define TOGGLE_LED6 \
    {if (bit_is_set(PORTD, LED6)) PORTD&=~(_BV(LED6)); else PORTD|=_BV(LED6);}
#define TOGGLE_LED7 \
    {if (bit_is_set(PORTD, LED7)) PORTD&=~(_BV(LED7)); else PORTD|=_BV(LED7);}

#define LED1_SET_ON PORTB |= _BV(LED1)
#define LED2_SET_ON PORTD |= _BV(LED2)
#define LED3_SET_ON PORTD |= _BV(LED3)
#define LED4_SET_ON PORTD |= _BV(LED4)
#define LED5_SET_ON PORTB |= _BV(LED5)
#define LED6_SET_ON PORTD |= _BV(LED6)
#define LED7_SET_ON PORTD |= _BV(LED7)

#define LED1_SET_OFF PORTB &= ~(_BV(LED1))
#define LED2_SET_OFF PORTD &= ~(_BV(LED2))
#define LED3_SET_OFF PORTD &= ~(_BV(LED3))
#define LED4_SET_OFF PORTD &= ~(_BV(LED4))
#define LED5_SET_OFF PORTB &= ~(_BV(LED5))
#define LED6_SET_OFF PORTD &= ~(_BV(LED6))
#define LED7_SET_OFF PORTD &= ~(_BV(LED7))

#endif

#endif /* PROJECT_H_ */
