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
TEST    = $(BUILD)/test.elf
MAP     = $(BUILD)/rt.map
TMAP    = $(BUILD)/test.map
LINK    = link.ld
SRCS    = $(wildcard *.c)
ASMS    = $(wildcard *.S)
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.c.o) $(ASMS:.S=.S.o))

# Kernel-specific (non-test) files
KSRCS   = $(wildcard kern/*.c)
KOBJS   = $(OBJS) $(addprefix $(BUILD)/, $(KSRCS:.c=.c.o))

# Test-specific files
TSRCS   = $(wildcard test/*.c)
TOBJS   = $(OBJS) $(addprefix $(BUILD)/, $(TSRCS:.c=.c.o))

BUILD_DIRS = $(BUILD) $(BUILD)/test $(BUILD)/kern

.SUFFIXES:
.SECONDARY:
.PHONY: all clean install

all: $(MAIN) $(TEST)

$(MAIN): $(LINK) $(KOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(MAP) -o $@ $(KOBJS) $(LIBS)

$(TEST): $(LINK) $(TOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(TMAP) -o $@ $(TOBJS) $(LIBS)

$(BUILD)/%.c.o: $(BUILD)/%.c.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.c.s: $(BUILD)/%.c.i |$(BUILD_DIRS)
	$(CC) -S -o $@ $(CFLAGS) $<

$(BUILD)/%.c.i: %.c |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD)/%.S.o: $(BUILD)/%.S.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.S.s: %.S |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD_DIRS):
	mkdir $@

clean:
	rm -rf $(BUILD)

install: $(MAIN) $(TEST)
	cp $(MAIN) $$tftp && chmod a+r $$tftp/$(notdir $(MAIN))
	cp $(TEST) $$tftp && chmod a+r $$tftp/$(notdir $(TEST))

-include $(BUILD)/*.c.d
-include $(BUILD)/*.S.d
-include $(BUILD)/kern/*.c.d
-include $(BUILD)/test/*.c.d
