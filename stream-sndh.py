import serial
import sys
import time

data = []
for l in sys.stdin.readlines():
    sample = []
    _, ts, regs = l.split()
    sample.append(chr(int(ts[-4:-2], 16)))
    sample.append(chr(int(ts[-2:], 16)))

    en_regs = filter(lambda x:x[1] != '..', enumerate(regs.split('-')))
    sample.append(chr(len(en_regs)))
    for n,r in en_regs:
        sample.append(chr(n))
        sample.append(chr(int(r,16)))

    data.append(''.join(sample))
s = ''.join(data)

print "*****", len(s)
fd = serial.Serial("/dev/ttyUSB0", 1000000, stopbits=1, timeout=1)
time.sleep(2) # Waiting for arduino initialization
i = 0
while i < len(s):
    cnt = ord(fd.read())
    #cnt = cnt if i+cnt < len(s) else len(s)-i
    j = i+cnt
    print cnt, i, j
    chunk = s[i:j]
    #print [ord(c) for c in chunk]
    fd.write(chr(cnt) + chunk)
    i = j
fd.close()
