Title: ym2149-sndh

Atmega 328P Firmware code + Python scripts to play YM and SNDH files
on the YM2149.

To build the and flash the firmware:

    $ make flash

The repository includes a couple of scripts:

* `stream-ym.py` to play YM files on the YM2149 chip
* `stream-sndh.py` to play SNDH files on the YM2149 chip
* `stream-sndh-2.py` to play SNDH files on the YM2149 chip.

The `stream-sndh-2.py` is faster than `stream-sndh.py` but consumes
more CPU. It can be useful to play some 'digi' sounds.

Not all SNDH files are currently supported. More optimization would be
required.
