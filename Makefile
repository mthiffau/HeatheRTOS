CFLAGS_FILE = make/cflags

HOST    = arm-elf-
CC      = $(HOST)gcc
AS      = $(HOST)as
LD      = $(HOST)gcc
CFLAGS  = $(shell cat $(CFLAGS_FILE))
ASFLAGS = -mcpu=arm920t -mapcs-32 # always use full stack frames
LDFLAGS = -nostdlib -Wl,-init,main -Wl,-N
LIBS    = -lgcc
BUILD   = build

# Files
MAIN    = $(BUILD)/rt.elf
TEST    = $(BUILD)/test.elf
REPEATER= $(BUILD)/repeater.elf
MAP     = $(BUILD)/rt.map
TMAP    = $(BUILD)/test.map
RMAP    = $(BUILD)/repeater.map
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

# Repeater files
RSRCS   = $(wildcard repeater/*.c)
ROBJS   = $(OBJS) $(addprefix $(BUILD)/, $(RSRCS:.c=.c.o))

BUILD_DIRS = $(BUILD) $(BUILD)/test $(BUILD)/kern $(BUILD)/repeater

.SUFFIXES:
.SECONDARY:
.PHONY: all clean install

all: $(MAIN) $(TEST) $(REPEATER)

$(MAIN): $(LINK) $(KOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(MAP) -o $@ $(KOBJS) $(LIBS)

$(TEST): $(LINK) $(TOBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(TMAP) -o $@ $(TOBJS) $(LIBS)

$(REPEATER): $(LINK) $(ROBJS)
	$(LD) $(LDFLAGS) -T $(LINK) -Wl,-Map,$(RMAP) -o $@ $(ROBJS) $(LIBS)


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
	cp $(REPEATER) $$tftp && chmod a+r $$tftp/$(notdir $(REPEATER))

-include $(BUILD)/*.c.d
-include $(BUILD)/*.S.d
-include $(BUILD)/kern/*.c.d
-include $(BUILD)/test/*.c.d
