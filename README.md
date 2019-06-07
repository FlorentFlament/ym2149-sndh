# YM2149 sndh player

ATmega 328P Firmware code + Python scripts to play YM and SNDH files
on the YM2149.

On can find additional information related to this project on [my
blog][3].

## Prerequisites

A recent version of [sc68][2] installed on the system (for instance by
building the latest version from [the master branch][4]). Details on
[how to build sc68][5] can be found on Fuzzytek blog.

To build and upload the ATmega328P firmware, the following packages
are required (Package names on Ubuntu 19.04):

* gcc-avr
* avr-libc
* avr-dude

## Usage

To build and flash the firmware:

```
$ make flash
```

To play an sndh file on the YM2149:

```
$ sc68 --ym-engine=dump -qqqn <sndh-file> | python stream-sndh.py
```


## Details

The repository includes a couple of scripts:

* `stream-ym.py` to play YM files on the YM2149 chip
* `stream-sndh.py` to play SNDH files on the YM2149 chip
* `stream-sndh-2.py` to play SNDH files on the YM2149 chip.

The `stream-sndh-2.py` is faster than `stream-sndh.py` but consumes
more CPU. It can be useful to play some 'digi' sounds.

Not all SNDH files are currently supported. More optimization would be
required.


## Hardware design

The design supported is the following one:

<img src="ym2149-schematic-v2.png" alt="YM2149 schematic V2"/>

The following table provides the wiring:

| Other | YM2149F name | YM2149F pin# | Board pin | ATmega328P pin# | ATmega328P name |
|-------|--------------|--------------|-----------|-----------------|-----------------|
|       | DA0          | 37           | A0        | 23              | PC0             |
|       | DA1          | 36           | A1        | 24              | PC1             |
|       | DA2          | 35           | A2        | 25              | PC2             |
|       | DA3          | 34           | A3        | 26              | PC3             |
|       | DA4          | 33           | A4        | 27              | PC4             |
|       | DA5          | 32           | A5        | 28              | PC5             |
|       | DA6          | 31           | D6        | 10              | PD6             |
|       | DA7          | 30           | D7        | 11              | PD7             |
|       | BC1          | 29           | D12       | 16              | PB4             |
| VCC   | BC2          | 28           |           |                 |                 |
|       | BDIR         | 27           | D13       | 17              | PB5             |
| GND   | ^SEL         | 26           |           |                 |                 |
| LED0  |              |              | D3        |  1              | PD3             |
| LED1  |              |              | D11       | 15              | PB3             |
| LED2  |              |              | D5        |  9              | PD5             |
| CHAN0 | ANALOG CH A  | 4            |           |                 |                 |
| CHAN1 | ANALOG CH B  | 3            |           |                 |                 |
| CHAN2 | ANALOG CH C  | 38           |           |                 |                 |
| CLOCK | CLOCK        | 22           |           |                 |                 |
| TIP   | ANALOG ABC   |              |           |                 |                 |
| RING1 | ANALOG ABC   |              |           |                 |                 |
| RING2 | VSS(GND)     | 1            |           |                 |                 |
| SLEEVE| VSS(GND)     | 1            | GNDx      |                 |                 |
|       | VCC          | 40           | 5V        |                 |                 |

<img src="ym2149-pic-v2.png" alt="YM2149 driven by Arduino Nano V2"/>

A video of the [YM2149 playing a Relix tune][1] is available on Youtube.

[1]: https://www.youtube.com/watch?v=JjofS8wdNEY
[2]: https://sourceforge.net/projects/sc68/
[3]: http://www.florentflament.com/blog/playing-sndh-on-ym2149.html
[4]: https://sourceforge.net/p/sc68/code/HEAD/tree/
[5]: https://fuzzytek.ml/linux/sc68/
