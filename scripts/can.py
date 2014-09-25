import serial
import sys
import time

def print_res(r):
  print len(r), map(lambda y: hex(ord(y)), x)

def write_hex(*args):
  for i in args:
    ser.write(chr(i))
  
ser = serial.Serial(sys.argv[1], 833333, xonxoff = 0, rtscts = 0, timeout = 2)
ser.setRTS(0)
ser.setDTR(0)
time.sleep(.150)
ser.setDTR(1)
time.sleep(.5)
ser.setDTR(0)
ser.setRTS(1)
print 'a'
ser.write('\x50\x00')
time.sleep(.01)
print 'b'
#ser.read()
ser.write('\x53\x20' + '\xff\x1c\x00\x00' * 6 + '\xff\x1c\x00\xff\xff\x1c\x00\x00')
x = ser.read(2)
print_res(x)
write_hex(0x53, 0x20, 0x43, 0x1c, 0x00, 0x00)
#write_hex(0x53, 0x20, 0x00, 0x00, 0x00, 0x00)
for i in range(0, 3):
  write_hex(0xff, 0x1c, 0x00, 0x00)
write_hex(0x60, 0x00, 0x00, 0x00, 0x43, 0x1c, 0x00, 0x00)
#write_hex(0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
for i in range(0, 2):
  write_hex(0xff, 0x1c, 0x00, 0x00)
x = ser.read(2)
print 'filter res',
print_res(x)

time.sleep(.2)

write_hex(0x60, 0x0b, 0x40, 0x00, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x10, 0x00, 0x03, 0x01)
x = ser.read(2)
print 'first req res',
print_res(x)
while True:
  try:
    x = ser.read(2)
    print 'first data hdr',
    print_res(x)
    if len(x) > 0 and x[0] != chr(0x70):
      break
    x = ser.read(ord(x[1]))
    print 'data',
    print_res(x)
  except:
    break

ser.setRTS(0)
ser.close()
