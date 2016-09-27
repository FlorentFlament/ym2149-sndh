import serial
import sys
import time

fd = serial.Serial("/dev/ttyUSB0", 1000000, stopbits=1, timeout=0)
time.sleep(2) # Waiting for arduino initialization

data = ""
done = False
while not done:

    if len(data) < 2000:
	    l = sys.stdin.readline()
	    if not l:
	        done = True
	        break

	    sample = []
	    _, ts, regs = l.split()
	    sample.append(chr(int(ts[-4:-2], 16)))
	    sample.append(chr(int(ts[-2:], 16)))

	    en_regs = filter(lambda x:x[1] != '..', enumerate(regs.split('-')))
	    sample.append(chr(len(en_regs)))
	    for n,r in en_regs:
	        sample.append(chr(n))
	        sample.append(chr(int(r,16)))

            data += (''.join(sample))

    # Send data to Chip if ready
    if len(data) > 100:
        avail = fd.read()
        if avail:
            avail = ord(avail)
            print "avail:", avail
            ck_size = min(avail, len(data))
            print "ck_size:", ck_size
            print "len(data):", len(data)
            fd.write(chr(ck_size) + data[:ck_size])
            data = data[ck_size:]
