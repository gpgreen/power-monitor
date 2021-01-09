/*
 * sensor.c
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "sensor.h"

// adc channels
volatile uint8_t adc_channels;

// the channel being measured
int current_channel;

// array of adc values
uint16_t adc_values[MAX_ADC_PINS];

// flag for adc interrupt
volatile uint8_t adc_complete_event;

void
sensor_init(void)
{
    // PORTC setup PINS for input
    DDRC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));

    // turn off analog comparator and digital input buffer
    ACSR |= _BV(ACD);
    DIDR1 |= _BV(AIN1D)|_BV(AIN0D);

    // turn off digital input buffers
    DIDR0 |= _BV(ADC5D)|_BV(ADC4D)|_BV(ADC3D)|_BV(ADC2D)|_BV(ADC0D);

    // start the ADC, and enable interrupt, scale clock by 32
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS2)|_BV(ADPS0);
    // at start, no channels are in use
    current_channel = -1;       /* channel currently measured */
}

void
sensor_pre_power_down(void)
{
    // turn off adc
    ADCSRA = 0;

    // shutdown modules
    PRR |= _BV(PRADC);

    // turn on pull ups on adc ports
    PORTC |= (_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));
}

void
sensor_post_power_down(void)
{
    // startup adc
    PRR &= ~(_BV(PRADC));
    // initialize the ADC, and enable interrupt, scale clock by 8
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS2)|_BV(ADPS0);
    current_channel = -1;
    
    // turn off pull ups on ADC ports
    PORTC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));
}

void
sensor_state_machine(void)
{
    // if we now have a set of channels to look at,
    // set the current_channel and initiate measurement
    uint8_t curchannels = adc_channels;
    if (curchannels > 0 && current_channel < 0)
    {
        // find the first channel
        for (uint8_t i=0; i<MAX_ADC_PINS; i++)
        {
            if (curchannels & (1<<i))
            {
                ATOMIC_BLOCK(ATOMIC_FORCEON) {
                    current_channel = i;
                }
                break;
            }
        }
        ADMUX = current_channel | _BV(REFS0);

        // start a new conversion
        ADCSRA |= _BV(ADSC);
    }

    // adc measurement finished, get the next channel
    if (adc_complete_event)
    {
        // if no channels are requested, then finish
        if (curchannels == 0) {
            ATOMIC_BLOCK(ATOMIC_FORCEON) {
                current_channel = -1;
            }
        } else {
            // get the next channel
            uint8_t i = current_channel + 1;
        find_channel:
            while (i < MAX_ADC_PINS)
            {
                if (curchannels & (1<<i)) {
                    ATOMIC_BLOCK(ATOMIC_FORCEON) {
                        current_channel = i;
                    }
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
            // start new conversion, with AVCC as reference
            ADMUX = current_channel | _BV(REFS0);
            ADCSRA |= _BV(ADSC);
        }
        adc_complete_event = 0;
    } else {
        // zero out channels not used
        for (int i=0; i<MAX_ADC_PINS; i++)
            if ((curchannels & (1 << i)) != (1 << i))
                adc_values[i] = 0;
    }
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
    adc_complete_event = 1;
}
