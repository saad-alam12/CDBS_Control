#!/home/cdbs/CDBS_Control/venv/bin/python
import serial
import time

ser=serial.Serial('/dev/ttyUSB0', 9600, timeout=1)
print(ser.name)
data=ser.read(100)
print(data)
ser.close()