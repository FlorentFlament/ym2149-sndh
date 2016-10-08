import Queue
import serial
import sys
import threading
import time

class UART_manager(threading.Thread):

    def __init__(self, fd, q):
        super(UART_manager, self).__init__()
        self.__fd = fd
        self.__q = q

    def run(self):
        time.sleep(1) # Wait for buffer to fill up
        while not self.__q.empty():
            # Read number of bytes to send
            available = ord(self.__fd.read())
            available = (available<<8) + ord(self.__fd.read())
            sys.stdout.write("\x1b[2K\rYM_empty: {}\tPC_buffer: {}".format(available, self.__q.qsize()))
            sys.stdout.flush()
            chunk_size = min(available, self.__q.qsize())
            self.__fd.write(chr(chunk_size>>8 & 0xff) + chr(chunk_size & 0xff))
            self.__fd.read() # Ack
            self.__fd.write(''.join([self.__q.get() for _ in xrange(chunk_size)]))


def main():
    fd = serial.Serial("/dev/ttyUSB0", 1000000, timeout=2)
    q = Queue.Queue()
    um = UART_manager(fd, q)

    um.start()
    l = sys.stdin.readline()
    while l:
        if q.qsize() < 3000:
            _, ts, regs = l.split()
            q.put(chr(int(ts[-4:-2], 16)))
            q.put(chr(int(ts[-2:], 16)))
            en_regs = filter(lambda x:x[1] != '..', enumerate(regs.split('-')))
            for n,r in en_regs:
                q.put(chr(n))
                q.put(chr(int(r,16)))
            q.put('\xff')
            l = sys.stdin.readline()
        else:
            time.sleep(0.01)
    for c in '\x00\x00\x08\x00\x09\x00\x0a\x00\xff': # Turn off channels
        q.put(c)

    um.join()
    fd.close()
    print ""


main()
