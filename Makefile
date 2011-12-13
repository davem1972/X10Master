#NOTE: This file changes the fuse values of the chip to enable brown-out detection.
#fuse settings are hard-coded into the bottom lines; change them only with care.

PRG            = X10Master
SRC            = main.c usiTwiSlave.c ringbuffer.c
OBJ            = $(SRC:%.c=%.o)
MCU_TARGET     = attiny461 #attiny2313 
PROGRAMMER     = usbtiny #avrispmkII 
AVRDUDE_TARGET = t2313 
F_CPU 	       = 8000000
PORT           = usb

OPTIMIZE       = -Os

DEFS           = -DF_CPU=$(F_CPU)UL
LIBS           =

ifdef DEBUG
DEFS           += -DDEBUG=1
endif

CLI_TARGET     = x10cli
CLI_SRC        = x10cli.c
CLI_OBJ        = $(CLI_SRC:%.c=%.o)

# You should not have to change anything below here.

CC             = avr-gcc

# Override is only needed by avr-lib build system.

override CFLAGS        = -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
override LDFLAGS       = -Wl,-Map,$(PRG).map

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o $(PRG).elf *.eps *.png *.pdf *.bak *.hex *.bin *.srec
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)
	rm $(CLI_TARGET)

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec


ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@


# command to program chip (invoked by running "make install")
install: all
	avrdude -p $(AVRDUDE_TARGET) -c $(PROGRAMMER) -P $(PORT) -v -e -b 115200  \
	 -U lfuse:w:0xE4:m -U hfuse:w:0xDF:m \
	 -U efuse:w:0xff:m -U eeprom:w:$(PRG)_eeprom.hex  \
	 -U flash:w:$(PRG).hex	


# was lfuse:w:0x62

# Original fuse settings: 	
#	 -U lfuse:w:0x64:m  -U hfuse:w:0xDF:m	-U efuse:w:0xff:m	


# Rule to build command line tool
$(CLI_TARGET): $(CLI_SRC)
	$(CLI_CC) $(CLI_CFLAGS) $(CLI_LDFLAGS) -o $@ $^ $(CLI_LIBS)

