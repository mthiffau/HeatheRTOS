CC      = gcc
AS      = as
LD      = ld
CFLAGS  = -I. -Wall -Wextra -Werror
CFLAGS += -fPIC -mcpu=arm920t -msoft-float
ASFLAGS = -mcpu=arm920t -mapcs-32 # always use full stack frames
LDFLAGS = -init main -N -L$(GNUARM)/lib/gcc/arm-elf/4.0.2
LIBS    = -lgcc
BUILD   = build

#CFLAGS += -DNDEBUG

# Files
MAIN    = $(BUILD)/rt.elf
MAP     = $(BUILD)/rt.map
LINK    = link.ld
SRCS    = $(wildcard *.c)
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.c.o))

.SUFFIXES:
.SECONDARY:
.PHONY: all clean install

all: $(MAIN)

$(MAIN): $(LINK) $(OBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Map $(MAP) -o $@ $(OBJS) $(LIBS)

$(BUILD)/%.c.o: $(BUILD)/%.c.s |$(BUILD)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.c.s: $(BUILD)/%.c.i |$(BUILD)
	$(CC) -S -o $@ $(CFLAGS) $<

$(BUILD)/%.c.i: %.c |$(BUILD)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD):
	mkdir $@

clean:
	rm -rf $(BUILD)

install: $(MAIN)
	cp $(MAIN) $$tftp

-include $(BUILD)/*.c.d
