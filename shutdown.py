import gpiozero
import sys
import time
import os

shutdown_pin = 22
#mcu_running_pin = 16
shutdown_pulse_minimum = 600              # milliseconds
shutdown_wait_delay = 20                  # milliseconds

def main(args):
    # the shutdown pin
    shutdown_button = Button(shutdown_pin,
                             pull_up=False,   # pin should be low until shutdown signalled
                             hold_time=shutdown_pulse_minimum/1000)
#    # the "i am running" pin
#    running_device = gpiozero.DigitalOutputDevice(mcu_running_pin,
#                                                  active_high=True,
#                                                  initial_value=False)
#
#    # set initial state
#    running_device.on()

    sleep_interval = 1                             # start out with 1 second sleep
    while True:
        if shutdown_button.is_pressed():
            sleep_interval = shutdown_wait_delay / 1000
            if shutdown_button.is_held():
                # execute the shutdown command
                os.command("/usr/bin/sudo /sbin/poweroff")
                sys.exit(0)
        else:
            sleep_interval = 1
        time.sleep(sleep_interval)
        
if __name__ == '__main__':
    main(sys.args)
