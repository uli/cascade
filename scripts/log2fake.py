import sys
import re
write = True
rmsg = []
wmsg = []
for l in sys.stdin.readlines():
  if not ('SERIAL read data' in l or 'SERIAL write data' in l):
    continue
  l = l.strip()
  #print l
  byte = re.search(' data ([0-9A-F][0-9A-F])', l)
  byte = byte.groups()[0]
  if write and 'read' in l:
    write = False
    print '{{',','.join(wmsg)+', -1},',
    wmsg = []
  elif not write and 'write' in l:
    write = True
    print '{', ','.join(rmsg)+', -1}},'
    rmsg = []
  if write:
    wmsg += ['0x'+byte]
  else:
    rmsg += ['0x'+byte]
print '{', ','.join(rmsg)+', -1}},'
