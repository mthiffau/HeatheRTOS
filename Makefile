HOST    = arm-elf-
CC      = $(HOST)gcc
AS      = $(HOST)as
LD      = $(HOST)gcc
CFLAGS  = -nostdinc -I. -Wall -Wextra -Werror -fPIC -mcpu=arm920t -msoft-float -O2
ASFLAGS = -mcpu=arm920t -mapcs-32 # always use full stack frames
LDFLAGS = -nostdlib -Wl,-init,main -Wl,-N
LIBS    = -lgcc
BUILD   = build

# Files
MAIN    = $(BUILD)/rt.elf
MAP     = $(BUILD)/rt.map
LINK    = link.ld
SRCS    = $(wildcard *.c)
ASMS    = $(wildcard *.S)
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.c.o) $(ASMS:.S=.S.o))

.SUFFIXES:
.SECONDARY:
.PHONY: all clean install

all: $(MAIN)

$(MAIN): $(LINK) $(OBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(MAP) -o $@ $(OBJS) $(LIBS)

$(BUILD)/%.c.o: $(BUILD)/%.c.s |$(BUILD)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.c.s: $(BUILD)/%.c.i |$(BUILD)
	$(CC) -S -o $@ $(CFLAGS) $<

$(BUILD)/%.c.i: %.c |$(BUILD)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD)/%.S.o: $(BUILD)/%.S.s |$(BUILD)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.S.s: %.S |$(BUILD)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD):
	mkdir $@

clean:
	rm -rf $(BUILD)

install: $(MAIN)
	cp $(MAIN) $$tftp && chmod a+r $$tftp/$(notdir $(MAIN))

-include $(BUILD)/*.c.d
-include $(BUILD)/*.S.d
