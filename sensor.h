/*
 * sensor.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */
#ifndef SENSOR_H_
#define SENSOR_H_

#include <stdint.h>

#define MAX_ADC_PINS 6

// array of adc values
extern uint16_t adc_values[MAX_ADC_PINS];
// the mask of channels in use
extern volatile uint8_t adc_channels;

extern void sensor_init(void);
extern void sensor_pre_power_down(void);
extern void sensor_post_power_down(void);
extern void sensor_state_machine(void);

#endif /* SENSOR_H_ */
