/*
 * sensor-peripheral
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 *
 * This device will act as an SPI slave. It reads data from the
 * internal ADC These can then be retrieved by a SPI master
 * device. The device interface is register based. The SPI protocol
 * involves the master sending a byte that is the register address,
 * and then 2 more bytes. The slave returns a value in the second byte
 * corresponding to that value
 *
 * REGISTERS
 * 0x01 = adc channels requested
 *   second byte contains flags of which adc channels are requested, ie if bit 0 is set
 *   then adc channel 0 is requested and so on up to 5 channels.
 *   Third byte is zeros
 * 0x10-0x14 = retrieve adc value on the channel [address - 16], ie address 0x10 is adc channel 0
 *
 * SPI protocol is implemented using a state machine, transitions happen during
 * USI overflow interrupt, which happens when a byte is received over SPI
 * state 0 = waiting for address byte
 * state 1 = waiting for second byte
 * state 2 = waiting for third byte
 * state 3 = transfer finished
 *
 * PINOUT
 * ------
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
 *        LED4-|13 PD7        PB2 16|-SS
 *        LED5-|14 PB0        PB1 15|-LED6
 *             |                    |
 *             +--------------------+
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "sensor.h"

// Define pin labels
// these are all portB
#define MISO 4
#define MOSI 3
#define SCK 5
#define SS 2

// adc channels
volatile uint8_t adc_finished;
volatile uint8_t adc_channels;
// the channel being measured
int current_channel;

// array of adc values
uint16_t adc_values[5];

// spi
volatile uint8_t addr;          /* register address received via SPI */
volatile uint8_t spi_state;     /* current SPI state machine val */

void
sensor_init(void)
{
    // setup PINS for input, RESET is already set as input and pull up on
    // due to fuse setting
    DDRB &= ~(_BV(MOSI)|_BV(SCK)|_BV(SS));
    // set pullups on input pins
    PORTB |= (_BV(MOSI)|_BV(SCK)|_BV(SS));

    // PORTB setup PINS for output
    DDRB |= _BV(MISO);

    // start the ADC, and enable interrupt, scale clock by 8
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS1)|_BV(ADPS0);
    // at start, no channels are in use
    adc_channels = 0;           /* the channels enabled mask */
    current_channel = -1;       /* channel currently measured */
    adc_finished = 0;

    // set SPI hardware, enable interrupt
    SPCR = _BV(SPE)|_BV(SPIE);

    // SPI starting state
    spi_state = 0;
}

void
sensor_pre_power_down(void)
{
    // turn off adc
    ADCSRA = 0;
    // shutdown modules
    PRR |= (_BV(PRADC)|_BV(PRSPI));
}

void
sensor_post_power_down(void)
{
    // startup adc
    PRR &= ~(_BV(PRADC));
    // initialize the ADC, and enable interrupt, scale clock by 8
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS1)|_BV(ADPS0);
    current_channel = -1;
    adc_finished = 0;
    
    PRR &= ~(_BV(PRSPI));
    // set SPI hardware, enable interrupt
    SPCR = _BV(SPE)|_BV(SPIE);
}

void
sensor_state_machine(void)
{
    // if we now have a set of channels to look at,
    // start the measuring
    if (adc_channels > 0 && current_channel < 0)
    {
        // find the first channel
        for (uint8_t i=0; i<5; i++)
        {
            if (adc_channels & (1<<i))
            {
                current_channel = i;
                break;
            }
        }
        ADMUX = current_channel;

        // start a new conversion
        ADCSRA |= _BV(ADSC);
    }

    if (adc_finished)
    {
        // if no channels are requested, then finish
        if (adc_channels == 0) {
            current_channel = -1;
        } else {
            // get the next channel
            uint8_t i = current_channel + 1;
        find_channel:
            while (i < 5)
            {
                if (adc_channels & (1<<i)) {
                    current_channel = i;
                    break;
                }
                ++i;
            }
            // roll back to 0 if needed
            if (i == 5) {
                i = 0;
                goto find_channel;
            }
            // start new conversion
            ADMUX = current_channel;
            ADCSRA |= _BV(ADSC);
        }
        adc_finished = 0;
    }

    // reset SPI if we have just handled a transaction
    if (spi_state == 3)
        spi_state = 0;
}

/*
 * ADC complete interrupt
 * interrupt flag cleared by hardware
 */
ISR(ADC_vect)
{
    uint8_t low, high;
    low = ADCL;
    high = ADCH;
    adc_values[current_channel] = (high << 8) | low;
    adc_finished = 1;
}

/*
 * Serial Transfer Complete interrupt
 * interrupt flag cleared by hardware
 */
ISR(SPI_STC_vect)
{
    uint8_t recvd = SPDR;
    static uint8_t send2 = 0;

    switch (spi_state)
    {
    case 0: // first byte recvd, send second
        addr = recvd;
        if (addr >= 0x10 && addr <= 0x14)
        {
            SPDR = (uint8_t)(adc_values[addr-0x10] & 0xFF);
            send2 = (uint8_t)((adc_values[addr-0x10] & 0xFF00) >> 8);
        }
        else if (addr == 0x01)
        {
            SPDR = 0;
            send2 = 0;
        } else
        {
            SPDR = 0;
            send2 = 0;
        }
        spi_state = 1;
        break;
    case 1: // second byte recvd, send third
        if (addr == 0x1)
            adc_channels = recvd;
        SPDR = send2;
        spi_state = 2;
        break;
    default: // third byte recvd, end of transfer
        spi_state = 3;
        SPDR = 0;
        break;
    }
}
