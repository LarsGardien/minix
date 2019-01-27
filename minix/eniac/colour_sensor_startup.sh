#!/bin/sh
mknod /dev/i2c-4 c 138
mknod /dev/tcs34725.4.29 c 139

minix-service up /service/tca9548a -label tca9548a.3.70 -args 'bus=3 address=0x70'
minix-service up /service/mux_i2c -dev /dev/i2c-4 -label i2c.4 -args 'instance=4 parent-instance=3 channel=0 mux=tca9548a.3.70'
minix-service up /service/tcs34725 -dev /dev/tcs34725.4.29 -label tcs34725.4.29 -args 'bus=4 address=0x29'
