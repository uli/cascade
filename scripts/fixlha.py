import sys

a = open(sys.argv[1], "r+")
header_size = ord(a.read(1))
a.seek(0x16)
a.write('file')
cksum = 0
a.seek(2)
for i in range(0, header_size):
  cksum += ord(a.read(1))
a.seek(1)
cksum = cksum & 0xff
a.write(chr(cksum))
a.close()
