# power-monitor
Firmware for ATmega328P that monitors a push button to turn on/off
a switching power supply. Supplies a shutdown signal to another MCU
so that it can shutdown cleanly before the power supply is switched
off.

Also implements ADC measurements in response to SPI requests. The AVR
acts as an SPI peripheral, connected to SPI0 on the Raspberry Pi.

Incorporates a MCP2515 CAN controller and MCP2551 CAN transceiver
connected to SPI1 on the Raspberry Pi.

Intended for use with Raspberry PI boat computer project. The
corresponding hardware design is at: ![Chart Plotter Hat](https://github.com/gpgreen/chart_plotter_hat)

## Raspberry Pi scripts

The script `shutdown_monitor.py` is used to control and monitor
the GPIO pins on the Raspberry Pi to work in concert with this
firmware. This script is copied to /usr/bin. The systemd service file
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

## ATMega328P pinout

Prototype board using a Arduino Nano (or clone), with LED's
```
                 ATmega328P-28P3
             +--------------------+
             |                    |
       RESET-|1  PC6        PC5 29|-[HDWR_ID]
            -|2  PD0        PC4 27|-LED6
            -|3  PD1        PC3 26|-LED5
      BUTTON-|4  PD2        PC2 25|-ADC2
    SHUTDOWN-|5  PD3        PC1 24|-ADC1
      ENABLE-|6  PD4        PC0 23|-ADC0
         VCC-|7                 22|-GND
         GND-|8                 21|-AREF
     EXT CLK-|9  PB6            20|-AVCC
     EXT CLK-|10 PB7        PB5 19|-SCK
        LED4-|11 PD5        PB4 18|-MISO
        LED3-|12 PD6        PB3 17|-MOSI
        LED2-|13 PD7        PB2 16|-CS
        LED1-|14 PB0        PB1 15|-MCU_RUNNING
             |                    |
             +--------------------+
```
![Chart Plotter Hat] board
HDWR_ID only present on Rev C hat with CAN hardware
HDWR_ID is grounded on that hardware to allow identifying it in the
firmware initialization.
```
                    ATmega328P-32A
             +--------------------------+
             |                          |
         3.3-|4  VCC              PB0 12|-LED1
         3.3-|18 AVCC             PB1 13|-MCU_RUNNING
         CAP-|20 AREF             PB2 14|-CS
         GND-|3  GND              PB3 15|-MOSI
             |                    PB4 16|-MISO
          A6-|19 ADC6             PB5 17|-SCK
          A7-|22 ADC7             PB6  7|-SHUTDOWN
             |                    PB7  8|-EEPROM
            -|30 PD0                    |
            -|31 PD1              PC0 23|-A0
      BUTTON-|32 PD2              PC1 24|-A1
   [HDWR_ID]-|1  PD3              PC2 25|-A2
          EN-|2  PD4              PC3 26|-A3
            -|9  PD5              PC4 27|-A4
            -|10 PD6              PC5 28|-A5
            -|11 PD7              PC6 29|-RESET
             |                          |
             +--------------------------+
```

## Building and firmware load

This is a ![CMake](https://cmake.org) project. There is one option
to the build ```PROTOTYPE``` for the 28pin firmware with LED's for debugging.

### Prerequisites:
 - avr-gcc
 - avr-libc
 - avrdude
 - cmake
 - ninja
 - ![build-tools](https//github.com/gpgreen/avfirmware-build-tools)

### Configure
To configure the firmware for a hat hardware build run cmake as follows:
```
cmake -G ninja -DCMAKE_TOOLCHAIN_FILE=<build-tools>/cmake-avr/generic-gcc-avr.cmake
```

To configure the firmware for a prototype build run cmake as follows:
```
cmake -G ninja -DPROTOTYPE=ON -DCMAKE_TOOLCHAIN_FILE=<build-tools>/cmake-avr/generic-gcc-avr.cmake
```

To see all targets:
```
ninja help
```

### Build
```
ninja
```

### Install firmware
```
ninja upload-power-monitor
```

## Code

![state machine for power monitoring](StateMachine.png)

Diagram made using [gaphor](https://gaphor.readthedocs.io/en/latest/)
