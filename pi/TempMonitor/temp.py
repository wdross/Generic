#!/usr/bin/env python
# encoding: utf-8

# just a quick and dirty program to demonstrate access to
# the DS1631 i2c thermometer attached to i2c bus 1 on
# address 0x48

import smbus
import time
bus = smbus.SMBus(1)
address = 0x48

def temp():
        t = bus.read_byte_data(address, 0xaa)
        return t

bus.write_byte(address, 0x51)  # Start converting
while True:                    # repeat forever (Control-C to stop it)
        print temp()
        time.sleep(1)          # wait a second
