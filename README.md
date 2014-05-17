BUILDING AND BOOTING
--------------------

As currently checked out fresh, the code is configured to build for
beaglebone black.

To compile the code, run 'make'. The arm-none-eabi- toolchain is
assumed, edit the Makefile if you are using something else. However,
I've only built it with the eabi gcc, so no promises it will work.

Once the code is done compiling, a directory called "build" will have
been created. It contains both hrtos.elf and a uImage file. I will
discuss now how to load the uImage file with uBoot.

Power on the beaglebone, with your serial debug cable/terminal set
up. Press a key to stop the uBoot countdown when prompted. Once at the
uBoot prompt, enter the following two commands:

set loadaddr 0x40300000
loady

The first tells uBoot to load the image it receives into the on-chip
SRAM in the am355x. The second tells it to accept an image file via
y-modem over the debug serial port. Use your serial terminal to dump
the file.

Once the file transfer has finished, enter the following command:

go 0x40300000

It is relatively simple to configure uBoot to automatically load the
same uImage file from the external SD card and run it. It can be done
from the uEnv.txt file.
