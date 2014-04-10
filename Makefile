DEVICE  = bbb

ifeq ($(DEVICE), ts7200)
CFLAGS_FILE = make/cflags-ts7200
HOST    = arm-none-eabi-
CC      = $(HOST)gcc
AS      = $(HOST)as
LD      = $(HOST)gcc
OCOPY   = $(HOST)objcopy
CFLAGS  = $(shell cat $(CFLAGS_FILE))
ASFLAGS = -mcpu=arm920t -mapcs-32 # always use full stack frames
LDFLAGS = -nostdlib -Wl,-init,main -Wl,-N
LIBS    = -lgcc
BUILD   = build

LINK    = make/link-ts7200.ld
endif

ifeq ($(DEVICE), bbb)
CFLAGS_FILE = make/cflags-bbb
HOST    = arm-none-eabi-
CC      = $(HOST)gcc
AS      = $(HOST)as
LD      = $(HOST)gcc
OCOPY   = $(HOST)objcopy
CFLAGS  = $(shell cat $(CFLAGS_FILE))
ASFLAGS = -mcpu=cortex-a8
LDFLAGS = -nostdlib -Wl,-init,main -Wl,-N
LIBS    = -lgcc
BUILD   = build

LINK    = make/link-bbb.ld
endif

# Files
MAIN    = $(BUILD)/rt.elf
BIN     = $(BUILD)/u-boot.bin
TEST    = $(BUILD)/test.elf
MAP     = $(BUILD)/rt.map
TMAP    = $(BUILD)/test.map

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
	$(OCOPY) $(MAIN) -O binary $(BUILD)/uImage

$(TEST): $(LINK) $(TOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(TMAP) -o $@ $(TOBJS) $(LIBS)


$(BUILD)/%.c.o: $(BUILD)/%.c.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.c.s: $(BUILD)/%.c.i $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -S -o $@ $(CFLAGS) $<

$(BUILD)/%.c.i: %.c $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD)/%.S.o: $(BUILD)/%.S.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.S.s: %.S $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $<

$(BUILD_DIRS):
	mkdir $@

clean:
	rm -rf $(BUILD)

install: $(MAIN) $(TEST) $(REPEATER)
	cp $(MAIN) $$tftp && chmod a+r $$tftp/$(notdir $(MAIN))
	cp $(TEST) $$tftp && chmod a+r $$tftp/$(notdir $(TEST))

-include $(BUILD)/*.c.d
-include $(BUILD)/*.S.d
-include $(BUILD)/kern/*.c.d
-include $(BUILD)/test/*.c.d
