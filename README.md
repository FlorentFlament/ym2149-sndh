Title: arduino-echo

This code is for testing bi-directional communication between a PC
and an Arduino board over USB at 1Mbits / second.

Prerequisite:
* The [Atmel tool chain][1]
* the `pyserial` package

To build and upload the firmware to the Arduino:

  $ make flash

To test the serial line:

  $ python test.py

[1]: http://www.florentflament.com/blog/arduino-hello-world-without-ide.html