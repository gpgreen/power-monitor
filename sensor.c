/*
 * sensor.c
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "project.h"
#include "power.h"
#include "sensor.h"

// adc channels
volatile uint8_t adc_finished;
volatile uint8_t adc_channels;
// the channel being measured
int current_channel;

// array of adc values
uint16_t adc_values[MAX_ADC_PINS];

void
sensor_init(void)
{
    // PORTC setup PINS for input
    DDRC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));

    // start the ADC, and enable interrupt, scale clock by 8
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS1)|_BV(ADPS0);
    // at start, no channels are in use
    adc_channels = 0;           /* the channels enabled mask */
    current_channel = -1;       /* channel currently measured */
    adc_finished = 0;
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
    ADCSRA = _BV(ADEN)|_BV(ADIE)|_BV(ADPS1)|_BV(ADPS0);
    current_channel = -1;
    adc_finished = 0;
    
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
        ADMUX = current_channel;

        // start a new conversion
        ADCSRA |= _BV(ADSC);
    }

    // adc measurement finished, get the next channel
    if (adc_finished)
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
            // start new conversion
            ADMUX = current_channel;
            ADCSRA |= _BV(ADSC);
        }
        adc_finished = 0;
        if (prev_state == Idle || prev_state == IdleEntry)
            change_state(IdleEntry);
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
    adc_finished = 1;
}
