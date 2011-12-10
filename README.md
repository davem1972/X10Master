X10Master
=========

X10 Master for use with PWC05 based X10 power line interface.  Implements an i2c slave for communication with the host processor.

Usage
-----

Requires avr-gcc tools for building with the Makefile, also has an AVR Studio 5 project (Windows only) for use with the AVR Dragon programmer (which allows debugging).

Tools
-----

In the tools folder there exists a Linux based command line tool for communicating with the X10Master via the i2c interface.

API
---

The X10 Master implements the following commands:


	X10_MASTER_COMMAND_PING           0x01
	X10_MASTER_COMMAND_UPTIME         0x02
	X10_MASTER_COMMAND_STATUS		  0x03
	X10_MASTER_COMMAND_READLOG        0x04
	X10_MASTER_COMMAND_X10_SENDCODE   0x05

Include commands.h in the source code.
