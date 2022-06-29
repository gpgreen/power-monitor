/*
 * sensor.c
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "sensor.h"

// adc channels (bitmask for selected channel(s))
volatile uint8_t adc_channels;

// the channel being measured currently
int8_t current_channel;

// array of measured adc values
uint16_t adc_values[MAX_ADC_PINS];

// flag for adc interrupt
volatile uint8_t adc_complete_event;

// turn on ADC, enable interrupt, scale clock by 128
const uint8_t k_adc_status_reg = _BV(ADEN)|_BV(ADIE)|_BV(ADPS2)|_BV(ADPS1)|_BV(ADPS0);

void
sensor_init(void)
{
#ifdef USE_28PIN
    // PORTC setup PINS for input
    DDRC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2));
#else
    // PORTC setup PINS for input
    DDRC &= ~(_BV(PC0)|_BV(PC1)|_BV(PC2)|_BV(PC3)|_BV(PC4)|_BV(PC5));
#endif
    // turn off analog comparator and digital input buffer
    ACSR |= _BV(ACD);
    DIDR1 |= _BV(AIN1D)|_BV(AIN0D);

    // turn off digital input buffers
#ifdef USE_28PIN
    DIDR0 |= _BV(ADC0D)|_BV(ADC1D)|_BV(ADC2D);
#else
    DIDR0 |= _BV(ADC0D)|_BV(ADC1D)|_BV(ADC2D)|_BV(ADC3D)|_BV(ADC4D)|_BV(ADC5D);
#endif

    // init ADC
    ADCSRA = k_adc_status_reg;

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
}

void
sensor_post_power_down(void)
{
    // startup adc
    PRR &= ~(_BV(PRADC));

    // init ADC
    ADCSRA = k_adc_status_reg;
    current_channel = -1;
}

void
sensor_state_machine(void)
{
    // if we now have a set of channels to look at,
    // set the current_channel and initiate measurement
    uint8_t curchannels = adc_channels;

    if (current_channel < 0 && curchannels > 0)
    {
        // find the first channel
        for (uint8_t i=0; i<MAX_ADC_PINS; i++)
        {
            if (curchannels & _BV(i))
            {
                current_channel = i;
                break;
            }
        }
        // start conversion
        ADMUX = current_channel | _BV(REFS0);
        ADCSRA |= _BV(ADSC);
    }

    // adc measurement finished, get the next channel
    if (adc_complete_event)
    {
        // if no channels are requested, then finish
        if (curchannels == 0) {
            current_channel = -1;
        } else {
            // get the next channel
            int i = current_channel + 1;
        find_channel:
            while (i < MAX_ADC_PINS)
            {
                if (curchannels & _BV(i)) {
                    current_channel = i;
                    break;
                }
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
            if ((curchannels & _BV(i)) == 0)
                adc_values[i] = 0;
    }
}

/*
 * ADC complete interrupt
 * interrupt flag cleared by hardware
 */
ISR(ADC_vect)
{
    uint8_t low = ADCL;
    uint8_t high = ADCH;
    adc_values[current_channel] = (high << 8) | low;
    adc_complete_event = 1;
}
