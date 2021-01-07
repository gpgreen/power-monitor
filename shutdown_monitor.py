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

# environment variables
k_pkill_delay = "OPENCPN_PKILL_DELAY"
k_pkill_user = "OPENCPN_USER"

# raspberry pi gpio pin numbers and delay constants
shutdown_pin = 22
mcu_running_pin = 23
shutdown_pulse_minimum = 600              # milliseconds
shutdown_wait_delay = 20                  # milliseconds

def main(args):
    # get environment variable(s)
    if os.environ.has_key(k_pkill_delay):
        opencpn_pkill_delay = int(os.environ[k_pkill_delay])
    else:
        print("{} environment variable not defined, quitting".format(k_pkill_delay))
        sys.exit(1)
    if os.environ.has_key(k_pkill_user):
        opencpn_user = os.environ[k_pkill_user]
    else:
        print("{} environment variable not defined, quitting".format(k_pkill_user))
        sys.exit(1)
    
    # the shutdown pin, active state is high
    shutdown_button = gpiozero.Button(shutdown_pin,
                                      pull_up=None,
                                      active_state=True,
                                      hold_time=shutdown_pulse_minimum/1000.0)

    # the "i am running" pin
    running_device = gpiozero.DigitalOutputDevice(mcu_running_pin,
                                                  active_high=True,
                                                  initial_value=True)

    # set initial state
    running_device.on()

    sleep_interval = 1                             # start out with 1 second sleep
    while True:
        if shutdown_button.is_pressed:
            sleep_interval = shutdown_wait_delay / 1000.0
            if shutdown_button.is_held:
                print("Detected shutdown signal, powering off..")
                os.system("/usr/bin/pkill -u {} opencpn".format(opencpn_user))
                time.sleep(opencpn_pkill_delay)
                # execute the shutdown command
                os.system("/usr/bin/sudo /sbin/poweroff")
                sys.exit(0)
        else:
            sleep_interval = 1
        time.sleep(sleep_interval)
        
if __name__ == '__main__':
    main(sys.argv)
