/*
 * spi.c
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
 * 0x01 = turn on adc channels
 *   second byte contains flags of which adc channels are requested, ie if bit 0 is set
 *   then adc channel 0 is requested and so on up to 5 channels.
 *   Third byte is zero
 * 0x02 = adc channels reading
 *   second byte contains which channels are activated
 *   third byte is zero
 * 0x03 = toggle EEPROM pin
 *   second and third byte 0
 * 0x10-0x14 = retrieve adc value on the channel [address - 16], ie address 0x10 is adc channel 0
 *
 * SPI protocol is implemented using a state machine, transitions happen during
 * spi transfer complete interrupt, which happens when a byte is received over SPI
 *
 * state 0 = waiting for address byte
 * state 1 = waiting for second byte
 * state 2 = waiting for third byte
 *
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "project.h"
#include "spi.h"
#include "sensor.h"

// toggling of eeprom
volatile uint8_t toggle_eeprom;

void
spi_init(void)
{
    // setup PINS for input, RESET is already set as input and pull up on
    // due to fuse setting
    SPI_DIR &= ~(_BV(MOSI)|_BV(SCK)|_BV(CS));

    // set pullups on input pins
    SPI_PORT |= (_BV(MOSI)|_BV(SCK)|_BV(CS));

    // PORTB setup PINS for output
    SPI_DIR |= _BV(MISO);

    // EEPROM to input, no pullup
    EEPROM_DIR &= ~_BV(EEPROM);
    
    // enabled SPI, enable interrupt
    SPCR = _BV(SPE)|_BV(SPIE);
}

void
spi_pre_power_down(void)
{
    // shutdown modules
    PRR |= _BV(PRSPI);

    // MISO to input, pull-up on
    SPI_DIR &= ~(_BV(MISO));
    SPI_PORT |= _BV(MISO);
}

void
spi_post_power_down(void)
{
    // MISO to output, pull-up off
    SPI_PORT &= ~(_BV(MISO));
    SPI_DIR |= _BV(MISO);

    // turn on SPI
    PRR &= ~(_BV(PRSPI));
    // enable SPI, enable interrupt
    SPCR = _BV(SPE)|_BV(SPIE);
}

void
spi_state_machine(void)
{
    // toggle eeprom if chosen
    if (toggle_eeprom)
    {
        if (bit_is_clear(EEPROM_PIN, EEPROM))
        {
            // set to input
            EEPROM_DIR &= ~_BV(EEPROM);
        } else {
            // set to output, set low
            EEPROM_DIR |= _BV(EEPROM);
            EEPROM_PORT &= _BV(EEPROM);
        }
        toggle_eeprom = 0;
    }
}

/*
 * Serial Transfer Complete interrupt
 * interrupt flag cleared by hardware
 */
ISR(SPI_STC_vect)
{
    uint8_t recvd = SPDR;
    static uint8_t send2 = 0;
    static uint8_t addr = 0;
    static int spi_state = 0;

    switch (spi_state)
    {
    case 0: // first byte recvd, send second
        addr = recvd;
        if (addr >= 0x10 && addr < MAX_ADC_PINS + 0x10)
        {
            SPDR = (uint8_t)(adc_values[addr-0x10] & 0xFF);
            send2 = (uint8_t)((adc_values[addr-0x10] & 0xFF00) >> 8);
        } else if (addr == 0x02)
        {
            SPDR = adc_channels;
            send2 = 0;
        } else if (addr == 0x3) {
            SPDR = 0;
            send2 = 0;
            toggle_eeprom = 1;
        }
        spi_state = 1;
        break;
    case 1: // second byte recvd, send third
        SPDR = send2;
        if (addr == 0x1)
            adc_channels = recvd;
        spi_state = 2;
        break;
    case 2:
        SPDR = 0;
        spi_state = 0; // third byte recvd, end of transfer
        break;
    }
#ifdef USE_LED
    TOGGLE_LED5;
#endif
}
