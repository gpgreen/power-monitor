# power-monitor
Firmware for ATtiny12 that monitors a push button to turn on/off
a switching power supply. Supplies a shutdown signal to another MCU
so that it can shutdown cleanly before the power supply is switched off.
Intended for use with Raspberry PI boat computer project.

Makefile is setup to use avr-gcc, avr-libc, and avrdude to compile and program the chip respectively. 

The compiled firmware (power.hex) can be programmed into the hardware using
```
avrdude -p attiny12 -c <your programmer here> -U flash:w:power.hex
```

This can also be done with the makefile target 'program', or `make program`

