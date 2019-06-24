#!/usr/bin/env python3

import argparse
import struct
import serial
import sys

DEBUG = False

class YmReader:
    def __parse_extra_infos(self):
        def readcstr():
            l = []
            c = self.__fd.read(1)[0]
            while c != 0:
                l.append(c)
                c = self.__fd.read(1)[0]
            return bytes(l).decode('ascii')
        self.__header['song_name'] = readcstr()
        self.__header['author_name'] = readcstr()
        self.__header['song_comment'] = readcstr()

    def __parse_header(self):
        # See:
        # http://leonard.oxg.free.fr/ymformat.html
        # ftp://ftp.modland.com/pub/documents/format_documentation/Atari%20ST%20Sound%20Chip%20Emulator%20YM1-6%20(.ay,%20.ym).txt
        ym_header = '> 4s 8s I I H I H I H'
        sz = struct.calcsize(ym_header)
        s = self.__fd.read(sz)
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
        regs = [self.__fd.read(cnt) for i in range(16)]
        # Storing the data in a list to be able to iterate on it several times
        self.__data = list(zip(*regs))

    def __read_data(self):
        if not self.__header['interleaved']:
            raise Exception(
                'Unsupported file format: Only interleaved data are supported')
        self.__read_data_interleaved()

    def __check_eof(self):
        if self.__fd.read(4) != b'End!':
            print('*Warning* End! marker not found after frames')

    def __init__(self, ym_fname):
        self.__fd = open(ym_fname, 'rb')
        self.__parse_header()
        self.__data = []

    def dump_header(self):
        for k in ('id','check_string', 'nb_frames', 'song_attributes',
                  'nb_digidrums', 'chip_clock', 'frames_rate', 'loop_frame',
                  'extra_data', 'song_name', 'author_name', 'song_comment'):
            print("{}: {}".format(k, self.__header[k]))

    def dump_data(self):
        for d in self.get_data():
            print(" ".join([("${:02x}".format(v)) for v in d]))

    def get_header(self):
        return self.__header

    def get_data(self):
        if not self.__data:
            self.__read_data()
            self.__check_eof()
            self.__fd.close()
        return self.__data


class ChipController:
    def __parse_ym(self, ym_data):
        self.__data = []
        ts = 0
        for d in ym_data:
            self.__data.append(ts>>8 & 0xff)
            self.__data.append(ts & 0xff)
            # r0-r12 are updated on YM all the time
            for i,v in enumerate(d[:13]):
                self.__data.append(i)
                self.__data.append(v)
            # r13 (Waveform shape) is only updated if != 0xff
            if d[13] != 0xff:
                self.__data.append(13)
                self.__data.append(d[13])
            # Not implementing DD (digi-drums) and TS (Timer-Synth)
            # Using bytes 14 and 15 of YM stream
            self.__data.append(0xff) # Stop character
            ts = (ts + 40000) & 0xffff if self.__firmware == 'slow' else 40000

    def __init__(self, ym_data, firmware='slow'):
        self.__firmware = firmware
        self.__parse_ym(ym_data)

    def get_data(self):
        return self.__data

    def get_length(self):
        return len(self.__data)

    def dump_stream(self):
        ln = []
        i = 0
        for n in self.__data:
            ln.append(n)
            if i == 1:
                ts = (ln[0]<<8) + ln[1]
                print(str(ts).ljust(5), end="  ")
            if i != 0 and i%2 == 0 and n == 0xff:
                print(" ".join([("${:02x}".format(v)) for v in ln]))
                ln = []
                i = 0
            else:
                i += 1

    def send_stream(self, fname):
        firmware_bitrate = {'slow': 1000000, 'fast': 500000}
        fd = serial.Serial(fname, firmware_bitrate[self.__firmware], timeout=2)
        # !!! https://github.com/torvalds/linux/blob/c05c2ec96bb8b7310da1055c7b9d786a3ec6dc0c/drivers/usb/serial/ch341.c
        # /* Unimplemented:
        #  * (cflag & CSIZE) : data bits [5, 8]
        #  * (cflag & PARENB) : parity {NONE, EVEN, ODD}
        #  * (cflag & CSTOPB) : stop bits [1, 2]
        #  */

        s = bytes(self.get_data())
        i = 0
        while i < len(s):
            cnt = fd.read(1)[0] << 8
            cnt+= fd.read(1)[0]
            if self.__firmware == 'slow':
                cnt = min(cnt, len(s)-i)
                fd.write([cnt >> 8, cnt & 0xff])
                fd.read(1) # ACK
            fd.write(s[i: i+cnt])
            i += cnt
        fd.close()


def main():
    parser = argparse.ArgumentParser(
        description='Play a YM file on a YM-ATmega board')
    parser.add_argument('ym_fname',
                        help='The path of the YM file to play')
    parser.add_argument('--device', default='/dev/ttyUSB0',
                        help='The device where to send the music stream (default /dev/ttyUSB0)')
    parser.add_argument('--firmware', choices=['slow', 'fast'], default='slow',
                        help='Specify which firmware to target')
    args = parser.parse_args()

    if DEBUG:
        print("\nStarting parsing of {}".format(args.ym_fname))

    ym = YmReader(args.ym_fname)
    ym.dump_header()
    if DEBUG:
        print("\nParsed {} successfully".format(args.ym_fname))
        ym.dump_data()
        print("\nBuilding board data stream using firmware {}".format(args.firmware))

    chip = ChipController(ym.get_data(), args.firmware)
    if DEBUG:
        print("\nBuilt board data stream successfully")
        chip.dump_stream()

    print("Data size: {}".format(chip.get_length()))
    chip.send_stream(args.device)

if __name__ == '__main__':
    main()
