# Set Target Device
DEVICE  = bbb

HARD_FLOAT = on

# Beaglebone Black Device Config
ifeq ($(DEVICE), bbb)
CFLAGS_FILE = make/cflags-bbb
ASFLAGS_FILE = make/asflags-bbb
# Hard float options
CFLAGSHF_FILE = make/cflagshf-bbb
ASFLAGSHF_FILE = make/asflagshf-bbb

HOST    = arm-none-eabi-
CC      = $(HOST)gcc
AS      = $(HOST)as
LD      = $(HOST)gcc
OCOPY   = $(HOST)objcopy

CFLAGS  = $(shell cat $(CFLAGS_FILE))
ASFLAGS = $(shell cat $(ASFLAGS_FILE))
ifeq ($(HARD_FLOAT), on)
CFLAGSHF = $(shell cat $(CFLAGSHF_FILE))
ASFLAGSHF = $(shell cat $(ASFLAGSHF_FILE))
CFLAGS := $(CFLAGS) $(CFLAGSHF)
ASFLAGS := $(ASFLAGS) $(ASFLAGSHF)
endif

LDFLAGS = -nostdlib -Wl,-init,main -Wl,-N
LIBS    = -lgcc
BUILD   = build

LINK    = make/link-bbb.ld
endif

# Architecture specific code
ARCH    = arch/$(DEVICE)

# Code for application processes
APPS    = apps/$(DEVICE)

INC = -I. -I$(ARCH) -I$(APPS)

# Files
MAIN    = $(BUILD)/rt.elf
BIN     = $(BUILD)/u-boot.bin
TEST    = $(BUILD)/test.elf
MAP     = $(BUILD)/rt.map
TMAP    = $(BUILD)/test.map

SRCS    = $(wildcard *.c) $(wildcard $(ARCH)/*.c) $(wildcard $(APPS)/*.c)
ASMS    = $(wildcard $(ARCH)/*.S) $(wildcard ./*.S)
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.c.o) $(ASMS:.S=.S.o))

# Kernel-specific (non-test) files
KSRCS   = $(wildcard kern/*.c)
KOBJS   = $(OBJS) $(addprefix $(BUILD)/, $(KSRCS:.c=.c.o))

# Test-specific files
TSRCS   = $(wildcard test/*.c)
TOBJS   = $(OBJS) $(addprefix $(BUILD)/, $(TSRCS:.c=.c.o))

BUILD_DIRS = $(BUILD) $(BUILD)/test $(BUILD)/kern $(BUILD)/$(ARCH) $(BUILD)/$(APPS)

.SUFFIXES:
.SECONDARY:
.PHONY: all clean

all: $(MAIN)

$(MAIN): $(LINK) $(KOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(MAP) -o $@ $(KOBJS) $(LIBS)
	$(OCOPY) $(MAIN) -O binary $(BUILD)/uImage

$(TEST): $(LINK) $(TOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(TMAP) -o $@ $(TOBJS) $(LIBS)


$(BUILD)/%.c.o: $(BUILD)/%.c.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $<

$(BUILD)/%.c.s: $(BUILD)/%.c.i $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -S -o $@ $(CFLAGS) $(INC) $<

$(BUILD)/%.c.i: %.c $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $(INC) $<

$(BUILD)/%.S.o: $(BUILD)/%.S.s |$(BUILD_DIRS)
	$(AS) -o $@ $(ASFLAGS) $(INC) $<

$(BUILD)/%.S.s: %.S $(CFLAGS_FILE) |$(BUILD_DIRS)
	$(CC) -E -o $@ -MD -MT $@ $(CFLAGS) $(INC) $<

$(BUILD_DIRS):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.c.d
-include $(BUILD)/*.S.d
-include $(BUILD)/kern/*.c.d
-include $(BUILD)/test/*.c.d
