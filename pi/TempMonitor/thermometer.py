#!/usr/bin/env python
# encoding: utf-8

# script that defines a Class (DS161) with methods to access the
# various features of the chip as per the datasheet

import smbus  # available methods: http://wiki.erazor-zone.de/wiki:linux:python:smbus:doc
import time


class DS_Config:
    _9BIT = 0x00
    _10BIT = 0x04
    _11BIT = 0x08
    _12BIT = 0x0C
    ONESHOT = 0x01
    POL = 0x02
    NVB = 0x10
    TLF = 0x20
    TLH = 0x40
    DONE = 0x80

class DS1631:
    """ DS1631 thermometer interface. Uses smbus for communication with device. """
    def __init__(self, bus, address=0x48):
        self.bus = bus;
        self.address = address;

    def set_temp_reg(self, reg, value):
        if value >= 0:
            rawval = int(round(256*value)) & 0xfff0;
        else:
            rawval = (65536 + int(round(256*value))) & 0xfff0;
        self.bus.write_word_data(self.address, reg, rawval);

    def get_temp(self):      # returns degrees C
        t = self.bus.read_byte_data(self.address, 0xAA);
        if t > 127:
            t = t - 256;
        else:
            t = t;
        return t;
    
    def get_tl(self):        # reads the lower temperature register
        return self.get_temp_reg(0xA1);

    def set_tl(self, value): # sets a new lower temperature value
        self.set_temp_reg(0xA1, value);

    def get_th(self):        # reads the upper temperature register
        return self.get_temp_reg(0xA2);

    def set_th(self, value): # sets a new upper temperature value
        self.set_temp_reg(0xA2, value);

    def get_config(self):    # returns the current configuration byte
        return self.bus.read_byte_data(self.address, 0xAC);

    def set_config(self, config):
        self.bus.write_byte_data(self.address, 0xAC, config);

    def start_convert(self):
        self.bus.write_byte(self.address, 0x51);

    def stop_convert(self):
        self.bus.write_byte(self.address, 0x22);

    def is_done(self):
        return self.get_config() & DS_Config.DONE;


if __name__ == '__main__':

    try:
        ds1 = DS1631(smbus.SMBus(1), 0x48);
        # ds1.set_config(DS_Config._12BIT | DS_Config.ONESHOT | DS_Config.POL);
    except Exception, e:
        print "Thermometer 1: ",e;
        ds1 = None;
        
    print "Thermometer initialized"

    ds1.start_convert();
    print "started..."

    for i in range(25):
        tmp=ds1.get_temp();
        print " %d C" %(tmp);
        time.sleep(0.5)    # 1/2 second
