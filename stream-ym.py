#!/usr/bin/env python

import functools
import itertools
import struct
import serial
import sys
import time

class YmReader(object):

    def __parse_extra_infos(self):
        # Thanks http://stackoverflow.com/questions/32774910/clean-way-to-read-a-null-terminated-c-style-string-from-a-file
        toeof = iter(functools.partial(self.__fd.read, 1), '')
        def readcstr():
            return ''.join(itertools.takewhile('\0'.__ne__, toeof))
        self.__header['song_name'] = readcstr()
        self.__header['author_name'] = readcstr()
        self.__header['song_comment'] = readcstr()

    def __parse_header(self):
        # See:
        # http://leonard.oxg.free.fr/ymformat.html
        # ftp://ftp.modland.com/pub/documents/format_documentation/Atari%20ST%20Sound%20Chip%20Emulator%20YM1-6%20(.ay,%20.ym).txt
        ym_header = '> 4s 8s I I H I H I H'
        s = self.__fd.read(struct.calcsize(ym_header))
        d = {}
        (d['id'],
         d['check_string'],
         d['nb_frames'],
         d['song_attributes'],
         d['nb_digidrums'],
         d['chip_clock'],
         d['frames_rate'],
         d['loop_frame'],
         d['extra_data'],
        ) = struct.unpack(ym_header, s)
        d['interleaved'] = d['song_attributes'] & 0x01 != 0
        self.__header = d

        if self.__header['nb_digidrums'] != 0:
            raise Exception(
                'Unsupported file format: Digidrums are not supported')
        self.__parse_extra_infos()

    def __read_data_interleaved(self):
        cnt  = self.__header['nb_frames']
        regs = [self.__fd.read(cnt) for i in xrange(16)]
        self.__data=[''.join(f) for f in zip(*regs)]

    def __read_data(self):
        if not self.__header['interleaved']:
            raise Exception(
                'Unsupported file format: Only interleaved data are supported')
        self.__read_data_interleaved()

    def __check_eof(self):
        if self.__fd.read(4) != 'End!':
            print '*Warning* End! marker not found after frames'

    def __init__(self, fd):
        self.__fd = fd
        self.__parse_header()
        self.__data = []

    def dump_header(self):
        for k in ('id','check_string', 'nb_frames', 'song_attributes',
                  'nb_digidrums', 'chip_clock', 'frames_rate', 'loop_frame',
                  'extra_data', 'song_name', 'author_name', 'song_comment'):
            print "{}: {}".format(k, self.__header[k])

    def get_header(self):
        return self.__header

    def get_data(self):
        if not self.__data:
            self.__read_data()
            self.__check_eof()
        return self.__data


def to_minsec(frames, frames_rate):
    secs = frames / frames_rate
    mins = secs / 60
    secs = secs % 60
    return (mins, secs)

def main():
    header = None
    data = None

    if len(sys.argv) != 3:
        print "Syntax is: {} <output_device> <ym_filepath>".format(sys.argv[0])
        exit(0)

    with open(sys.argv[2]) as fd:
        ym = YmReader(fd)
        ym.dump_header()
        header = ym.get_header()
        data = ym.get_data()

    data_ts = []
    ts = 0
    for d in data:
        data_ts.append(chr(ts>>8 & 0xff))
        data_ts.append(chr(ts & 0xff))
        for i,v in enumerate(d):
            data_ts.append(chr(i))
            data_ts.append(v)
        data_ts.append('\xff') # Stop character
        ts = (ts + 40000) & 0xffff

    s = ''.join(data_ts)
    print "Data size: {}".format(len(s))
    fd = serial.Serial(sys.argv[1], 1000000, timeout=2)
    # !!! https://github.com/torvalds/linux/blob/c05c2ec96bb8b7310da1055c7b9d786a3ec6dc0c/drivers/usb/serial/ch341.c
    # /* Unimplemented:
    #  * (cflag & CSIZE) : data bits [5, 8]
    #  * (cflag & PARENB) : parity {NONE, EVEN, ODD}
    #  * (cflag & CSTOPB) : stop bits [1, 2]
    #  */

    i = 0
    while i < len(s):
        h = fd.read()
        l = fd.read()
        cnt = (ord(h)<<8) + ord(l)
        fd.write(h)
        fd.write(l)
        fd.read() # ACK
        fd.write(s[i: i+cnt])
        i += cnt
    fd.close()

if __name__ == '__main__':
    main()
