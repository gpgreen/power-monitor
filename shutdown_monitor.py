#!/usr/bin/env python3
#
# Copyright 2020 Greg Green <ggreen@bit-builder.com>
#
# Script to detect a shutdown signal sent by the Chart Plotter Hat.
# If the signal is detected, then call the 'sudo poweroff' command
# to shutdown the Pi cleanly.

import gpiozero
import sys
import time
import os

# which user is running OpenCPN, so we can shut it down
opencpn_user = "pi"

shutdown_pin = 22
mcu_running_pin = 23
shutdown_pulse_minimum = 600              # milliseconds
shutdown_wait_delay = 20                  # milliseconds

def main(args):
    # the shutdown pin, active state is high, no pullups
    shutdown_button = gpiozero.Button(shutdown_pin,
                                pull_up=False,
                                hold_time=shutdown_pulse_minimum/1000.0)
    # Rev A of the hat has the mcu_running pin connected to the 3.3V pin, so it
    # is always on whenever the Pi is powered. This has been corrected in
    # Rev B. This still works, but the Hat power monitor cannot know that the
    # Pi is in the multi user state, which is when this script is intended
    # to start

    # the "i am running" pin
    running_device = gpiozero.DigitalOutputDevice(mcu_running_pin,
                                                  active_high=True,
                                                  initial_value=True)

    # set initial state
    running_device.on()

    sleep_interval = 1                             # start out with 1 second sleep
    while True:
        if shutdown_button.is_pressed():
            sleep_interval = shutdown_wait_delay / 1000.0
            if shutdown_button.is_held():
                print("Detected shutdown signal, powering off..")
                os.system("/usr/bin/pkill -u {} opencpn".format(opencpn_user))
                time.sleep(1)
                # execute the shutdown command
                os.system("/usr/bin/sudo /sbin/poweroff")
                sys.exit(0)
        else:
            sleep_interval = 1
        time.sleep(sleep_interval)
        
if __name__ == '__main__':
    main(sys.argv)
