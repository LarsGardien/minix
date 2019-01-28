#!/bin/sh
mknod /dev/i2c-4 c 138 0
mknod /dev/i2c-5 c 139 0
mknod /dev/i2c-6 c 140 0
mknod /dev/i2c-7 c 141 0
mknod /dev/i2c-8 c 142 0
mknod /dev/i2c-9 c 143 0
mknod /dev/i2c-10 c 144 0
mknod /dev/i2c-11 c 145 0
mknod /dev/tcs34725.4.29 c 146 0
mknod /dev/tcs34725.5.29 c 147 0
mknod /dev/tcs34725.6.29 c 148 0
mknod /dev/tcs34725.7.29 c 149 0
mknod /dev/tcs34725.8.29 c 150 0
mknod /dev/tcs34725.9.29 c 151 0
mknod /dev/tcs34725.10.29 c 152 0
mknod /dev/tcs34725.11.29 c 153 0

minix-service up /service/tca9548a -label tca9548a.3.70 -args 'bus=3 address=0x70'

minix-service up /service/mux_i2c -dev /dev/i2c-4 -label i2c.4 -args 'instance=4 parent-instance=3 channel=0 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-5 -label i2c.5 -args 'instance=5 parent-instance=3 channel=1 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-6 -label i2c.6 -args 'instance=6 parent-instance=3 channel=2 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-7 -label i2c.7 -args 'instance=7 parent-instance=3 channel=3 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-8 -label i2c.8 -args 'instance=8 parent-instance=3 channel=4 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-9 -label i2c.9 -args 'instance=9 parent-instance=3 channel=5 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-10 -label i2c.10 -args 'instance=10 parent-instance=3 channel=6 mux=tca9548a.3.70'
minix-service up /service/mux_i2c -dev /dev/i2c-11 -label i2c.11 -args 'instance=11 parent-instance=3 channel=7 mux=tca9548a.3.70'

minix-service up /service/tcs34725 -dev /dev/tcs34725b4s29 -label tcs34725.4.29 -args 'bus=4 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b5s29 -label tcs34725.4.29 -args 'bus=5 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b6s29 -label tcs34725.4.29 -args 'bus=6 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b7s29 -label tcs34725.4.29 -args 'bus=7 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b8s29 -label tcs34725.4.29 -args 'bus=8 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b9s29 -label tcs34725.4.29 -args 'bus=9 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b10s29 -label tcs34725.4.29 -args 'bus=10 address=0x29'
minix-service up /service/tcs34725 -dev /dev/tcs34725b11s29 -label tcs34725.4.29 -args 'bus=11 address=0x29'
