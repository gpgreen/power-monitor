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
kpkill_delay = "OPENCPN_PKILL_DELAY"
kpkill_user = "OPENCPN_USER"

# raspberry pi gpio pin numbers and delay constants
shutdown_pin = 22
mcu_running_pin = 23
shutdown_pulse_minimum = 600              # milliseconds
shutdown_wait_delay = 20                  # milliseconds

def main(args):
    # get environment variable(s)
    if os.environ.has_key(kpkill_delay):
        opencpn_pkill_delay = int(os.environ[kpkill_delay])
    else:
        print("{} environment variable not defined, quitting".format(kpkill_delay))
        sys.exit(1)
    if os.environ.has_key(kpkill_user):
        opencpn_user = os.environ[kpkill_user]
    else:
        print("{} environment variable not defined, quitting".format(kpkill_user))
        sys.exit(1)
    
    # the shutdown pin, active state is low, no pullups
    shutdown_button = gpiozero.Button(shutdown_pin,
                                      pull_up=None,
                                      active_state=False,
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
