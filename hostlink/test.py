#!/usr/bin/python

import time

hdr = bytearray()
for i in range (0, 16):
  hdr.append(0)

msg = bytearray()
for i in range (0, 16):
  msg.append(ord('A')+i)

f = open("/tmp/pciestream-in", "w")
f.write(hdr)
f.write(msg)
f.flush()
f.close()

f = open("/tmp/pciestream-out", "r")
got = f.read(16)
f.close()

print got
