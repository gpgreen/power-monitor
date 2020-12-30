# power-monitor
Firmware for ATmega328P that monitors a push button to turn on/off
a switching power supply. Supplies a shutdown signal to another MCU
so that it can shutdown cleanly before the power supply is switched
off.

Also implements ADC measurements in response to SPI requests.

Intended for use with Raspberry PI boat computer project. The
corresponding hardware design is at: ![Chart Plotter Hat](https://github.com/gpgreen/chart_plotter_hat)

## Raspberry Pi scripts

The script `shutdown_monitor.py` is used to control and monitor
the GPIO pins on the Raspberry Pi to work in concert with this
firmware. This script is copied to /usr/bin. A systemd service file is
`shutdown_monitor.service` and is copied to /lib/systemd/service.

Enabling and starting the service using systemd
```
systemctl enable shutdown_monitor.service
systemctl start shutdown_monitor.service
```

To disable the service from starting at boot
```
systemctl disable shutdown_monitor.service
```

## ATMega328P fuse settings
The fuses are changed from the default and need to be updated
```
avrdude -p atmega328p -c <your programmer here> -U lfuse:w:0xc2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
```

## Building and firmware load
Makefile is setup to use avr-gcc, avr-libc, and avrdude to compile and program the chip. 

The compiled firmware (power.hex) can be programmed into the hardware using
```
avrdude -p atmega328p -c <your programmer here> -U flash:w:power.hex
```

This can also be done with the makefile target 'program', or `make
program`


## Code

![state machine for power monitoring](https://github.com/gpgreen/power-monitor/blob/main/StateMachine.png)

Diagram made using ![gaphor](https://gaphor.readthedocs.io/en/latest/)
