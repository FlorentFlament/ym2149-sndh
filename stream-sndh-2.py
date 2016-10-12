import serial
import sys
import time


class RxStateMachine(object):
    # State machine definition
    def waiting(self):
        tmp = self.__fd.read()
        if tmp:
            self.__available = ord(tmp)<<8
            self.__state = "WAIT_LO"

    def wait_lo(self):
        tmp = self.__fd.read()
        if tmp:
            self.__available += ord(tmp)
            self.__state = "SENDING"

    def sending(self):
        if self.__available:
            sys.stdout.write("\x1b[2K\rYM_empty: {}\tPC_buffer: {}".format(self.__available, len(self.__data)))
            self.__ck_size = min(self.__available, len(self.__data))
            sys.stdout.flush()
            self.__fd.write(chr(self.__ck_size>>8 & 0xff))
            self.__fd.write(chr(self.__ck_size & 0xff))
        self.__state = "WAIT_ACK"

    def wait_ack(self):
        ack = self.__fd.read()
        if ack:
            self.__fd.write(self.__data[:self.__ck_size])
            self.__data = self.__data[self.__ck_size:]
            self.__state = "WAITING"

    def __init__(self, fd):
        self.__fd = fd
        self.__state = "WAITING"
        self.__available = 0
        self.__data = ""
        self.__ck_size = 0

        self.__state_machine = {
            "WAITING" : self.waiting,
            "WAIT_LO" : self.wait_lo,
            "SENDING" : self.sending,
            "WAIT_ACK" : self.wait_ack,
        }

    def next(self):
        self.__state_machine[self.__state]()

    def get_datalen(self):
        return len(self.__data)

    def put_data(self, data):
        self.__data += data


def main():
    sm = RxStateMachine(serial.Serial("/dev/ttyUSB0", 1000000, timeout=0))
    done = False
    while not done:
        if sm.get_datalen() < 5000:
            l = sys.stdin.readline()
            if not l:
                done = True
                break
            sample = []
            _, ts, regs = l.split()
            sample.append(chr(int(ts[-4:-2], 16)))
            sample.append(chr(int(ts[-2:], 16)))
            en_regs = filter(lambda x:x[1] != '..', enumerate(regs.split('-')))
            for n,r in en_regs:
                sample.append(chr(n))
                sample.append(chr(int(r,16)))
            sample.append('\xff')
            sm.put_data(''.join(sample))
        sm.next()

main()
