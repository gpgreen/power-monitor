/*
 * sensor.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */
#ifndef SENSOR_H_
#define SENSOR_H_

extern void sensor_init(void);
extern void sensor_pre_power_down(void);
extern void sensor_post_power_down(void);
extern void sensor_state_machine(void);

#endif /* SENSOR_H_ */
