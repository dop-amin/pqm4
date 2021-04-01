#!/usr/bin/env python3
import serial
import sys
import platform

dev = serial.Serial("/dev/ttyACM0", 115200)

print("> Returned data:", file=sys.stderr)

while True:
    x = dev.read()
    sys.stdout.buffer.write(x)
    sys.stdout.flush()
