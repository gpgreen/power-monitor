/*
 * spi.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */
#ifndef SPI_H_
#define SPI_H_

#include <stdint.h>

extern volatile uint8_t spi_stc_event;

extern void spi_init(void);
extern void spi_pre_power_down(void);
extern void spi_post_power_down(void);
extern void spi_state_machine(void);

#endif /* SPI_H_ */
