DEVKITARM ?= C:/devkitPro/devkitARM
PREFIX := $(DEVKITARM)/bin/arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size
PYTHON ?= python

TARGET := build/ellis_house_gba
CFLAGS := -mcpu=arm7tdmi -marm -mlong-calls -O3 -flto -ffast-math -ffreestanding -fno-builtin -fomit-frame-pointer -Wall -Wextra -Igenerated
LDFLAGS := -mcpu=arm7tdmi -marm -nostdlib -flto -Wl,-T,linker.ld,-Map,$(TARGET).map,--gc-sections

.PHONY: all clean assets
all: $(TARGET).gba
	cp -f $(TARGET).gba ../ellis_house_gba.gba

assets: generated/assets.s generated/assets.h

generated/assets.s generated/assets.h &: tools/generate_assets.py ../assets/game.manifest
	$(PYTHON) tools/generate_assets.py

build:
	mkdir build

build/start.o: source/start.s | build
	$(CC) $(CFLAGS) -c $< -o $@

build/main.o: source/main.c generated/assets.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/assets.o: generated/assets.s generated/assets.h | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: build/start.o build/main.o build/assets.o
	$(CC) $(LDFLAGS) $^ -lgcc -o $@
	$(SIZE) $@

$(TARGET).gba: $(TARGET).elf tools/fix_header.py
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) tools/fix_header.py $@

clean:
	-rmdir /s /q build 2>NUL
	-del /q generated\assets.s generated\assets.h generated\*.bin 2>NUL
