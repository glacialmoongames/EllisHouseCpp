.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set")
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules
TARGET := EllisHouse3DS
BUILD := build
SOURCES := source ../src
INCLUDES := include ../src
GRAPHICS := gfx
ROMFS := romfs
GFXBUILD := $(ROMFS)/gfx
META := meta
APP_TITLE := Elli's House
APP_DESCRIPTION := Native Nintendo 3DS port
APP_AUTHOR := Glacial Moon Games
BANNERTOOL ?= bannertool
MAKEROM ?= makerom
THREEDSTOOL ?= 3dstool
CIA_OUTPUT := ../EllisHouse.cia
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -O3 -flto -mword-relocations -ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -std=gnu++20 -fexceptions -frtti
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -flto -Wl,-Map,$(notdir $*.map),--gc-sections
LIBS := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
# The 3DS-specific main replaces the desktop entry point.
CPPFILES := $(filter-out main.cpp,$(CPPFILES)) main.cpp
GFXFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
export DEPSDIR := $(CURDIR)/$(BUILD)
export OFILES := $(CPPFILES:.cpp=.o)
export HFILES := $(GFXFILES:.t3s=.h)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export LD := $(CXX)
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS) --smdh=$(CURDIR)/$(TARGET).smdh
export APP_ICON := $(CURDIR)/$(META)/icon.png
export ROMFS_T3XFILES := $(patsubst %.t3s,$(GFXBUILD)/%.t3x,$(GFXFILES))
.PHONY: all clean assets cia
all: $(BUILD) $(GFXBUILD) $(ROMFS_T3XFILES) $(HFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@cp -f $(TARGET).3dsx ..
cia: all
	@"$(BANNERTOOL)" makesmdh -s "$(APP_TITLE)" -l "$(APP_TITLE)" -p "$(APP_AUTHOR)" -i "$(CURDIR)/$(META)/icon.png" -o "$(CURDIR)/$(META)/icon.smdh" -f visible,allow3d
	@"$(BANNERTOOL)" makebanner -i "$(CURDIR)/$(META)/banner.png" -a "$(CURDIR)/$(ROMFS)/assets/sounds/sfx_coin.wav" -o "$(CURDIR)/$(META)/banner.bnr"
	@rm -f "$(CURDIR)/$(META)/romfs.bin" "$(CIA_OUTPUT)"
	@"$(THREEDSTOOL)" -cvtf romfs "$(CURDIR)/$(META)/romfs.bin" --romfs-dir "$(CURDIR)/$(ROMFS)" >/dev/null
	@"$(MAKEROM)" -f cia -o "$(CIA_OUTPUT)" -elf "$(CURDIR)/$(TARGET).elf" -rsf "$(CURDIR)/$(META)/EllisHouse.rsf" -icon "$(CURDIR)/$(META)/icon.smdh" -banner "$(CURDIR)/$(META)/banner.bnr" -exefslogo -target t -romfs "$(CURDIR)/$(META)/romfs.bin"
assets:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/generate_assets.ps1
$(BUILD) $(GFXBUILD):
	@mkdir -p $@
clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh $(GFXBUILD) $(META)/icon.smdh $(META)/banner.bnr $(META)/romfs.bin
$(GFXBUILD)/%.t3x $(BUILD)/%.h: %.t3s
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x
else
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OFILES): $(HFILES)
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
