PREFIX ?= arm-none-linux-gnueabi-
CC = $(PREFIX)gcc
AR = $(PREFIX)ar
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
TARGET ?= arm
HOSTCC ?= gcc

CFLAGS ?= -Wall -Wno-unused-variable -Wno-address-of-packed-member -Wno-int-to-pointer-cast -Os
CFLAGS += -g -fPIC -Iinclude -Ilibgsfw/include -DLIBGSFW_EMBEDDED
LIBGCC = $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LDFLAGS = -Bstatic -T virgil.lds

BUILD = build/$(TARGET)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%,$(BUILD)/obj/%,$(SRCS:.c=.o))

.PHONY: all clean virgil

all: virgil

virgil : $(BUILD)/virgil.bin
	./libgsfw/build/native/mkboot $(BUILD)/virgil.bin virgil
	truncate -s %128 virgil

$(BUILD)/virgil.bin: $(BUILD)/virgil.elf
	$(OBJCOPY) --gap-fill=0xff -O binary $(BUILD)/virgil.elf $(BUILD)/virgil.bin
	truncate -s %4 $(BUILD)/virgil.bin

$(BUILD)/virgil.elf: $(OBJS)
	$(LD) $(LDFLAGS) --start-group $(OBJS) $(LIBGCC) --end-group -o $@

$(BUILD)/obj/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -nostdlib -o $@ $<

clean:
	rm -rf $(BUILD)
