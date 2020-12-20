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
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "project.h"
#include "sensor.h"

#define MAX_ADC_PINS 6

// adc channels
volatile uint8_t adc_finished;
volatile uint8_t adc_channels;
// the channel being measured
int current_channel;

// array of adc values
uint16_t adc_values[MAX_ADC_PINS];

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

    // PORTC setup PINS for input
    DDRC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));
    
    // start the ADC, and enable interrupt, scale clock by 8
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS1)|_BV(ADPS0);
    // at start, no channels are in use
    adc_channels = 0;           /* the channels enabled mask */
    current_channel = -1;       /* channel currently measured */
    adc_finished = 0;

    // enabled SPI, enable interrupt
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

    // MISO to input, pull-up on
    DDRB &= ~(_BV(MISO));
    PORTB |= _BV(MISO);

    // turn on pull ups on adc ports
    PORTC |= (_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));
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
    
    // MISO to output, pull-up off
    PORTB &= ~(_BV(MISO));
    DDRB |= _BV(MISO);

    // turn off pull ups on ADC ports
    PORTC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));

    // turn on SPI
    PRR &= ~(_BV(PRSPI));
    // enable SPI, enable interrupt
    SPCR = _BV(SPE)|_BV(SPIE);
}

void
sensor_state_machine(void)
{
    // if we now have a set of channels to look at,
    // set the current_channel and initiate measurement
    if (adc_channels > 0 && current_channel < 0)
    {
        // find the first channel
        for (uint8_t i=0; i<MAX_ADC_PINS; i++)
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

    // adc measurement finished, get the next channel
    if (adc_finished)
    {
        // if no channels are requested, then finish
        if (adc_channels == 0) {
            current_channel = -1;
        } else {
            // get the next channel
            uint8_t i = current_channel + 1;
        find_channel:
            while (i < MAX_ADC_PINS)
            {
                if (adc_channels & (1<<i)) {
                    current_channel = i;
                    break;
                } else
                    // zero out channels not used
                    adc_values[i] = 0;
                ++i;
            }
            // roll back to 0 if needed
            if (i >= MAX_ADC_PINS) {
                i = 0;
                goto find_channel;
            }
            // start new conversion
            ADMUX = current_channel;
            ADCSRA |= _BV(ADSC);
        }
        adc_finished = 0;
    } else {
        // zero out channels not used
        for (int i=0; i<MAX_ADC_PINS; i++)
            if ((adc_channels & (1 << i)) != (1 << i))
                adc_values[i] = 0;
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
        if (addr >= 0x10 && addr < MAX_ADC_PINS + 0x10)
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
