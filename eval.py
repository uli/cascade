# parse latency dumps

import struct
import sys
a = open(sys.argv[1], 'r').read()
c = list()
for i in range(0, len(a), 16):
  b = struct.unpack('HHIIBBBB', a[i:i+16])
  if b[3] > 1:	# only look at stuff that has been executed at least twice
    if True or b[2] > b[3] or b[0] > 0:
      # this thing consistently needs more than one microsecond
      print '!!!',
      print hex(i / 16), b
    c += [(hex(i / 16), b[0], b[1], b[2], b[3], float(b[2])/b[3])]

# cc[0]: address
# cc[1]: minimum latency
# cc[2]: maximum latency
# cc[3]: total latency
# cc[4]: total iterations
# cc[5]: average latency
d = sorted(c, key = lambda cc: cc[3])

# print the flop 10
for i in d[-100:]:
  print i
