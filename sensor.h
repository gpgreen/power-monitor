/*
 * sensor.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */
#ifndef SENSOR_H_
#define SENSOR_H_

#include <stdint.h>
#include "project.h"

#ifdef USE_28PIN
#define MAX_ADC_PINS 3
#else
#define MAX_ADC_PINS 8
#endif

// array of adc values
extern uint16_t adc_values[MAX_ADC_PINS];

// the mask of channels in use
extern volatile uint8_t adc_channels;

// adc finished interrupt
extern volatile uint8_t adc_complete_event;

extern void sensor_init(void);
extern void sensor_pre_power_down(void);
extern void sensor_post_power_down(void);
extern void sensor_state_machine(void);

#endif /* SENSOR_H_ */
