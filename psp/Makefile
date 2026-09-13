TARGET = EllisHousePSP
OBJS = source/main.o source/raylib_psp.o source/game.o source/manifest.o
INCDIR = include ../src
CFLAGS = -O3 -G0 -Wall -ffunction-sections -fdata-sections -fomit-frame-pointer -ffast-math -fno-math-errno -D__PSP__
# The shared game sources are built by explicit rules below. They must carry
# the optimisation flags as well; otherwise the PSP spends most of its frame
# in unoptimised C++ despite the CFLAGS setting above.
CXXFLAGS = $(CFLAGS) -std=gnu++20 -fexceptions -frtti -fno-threadsafe-statics
LDFLAGS = -Wl,-Ttools/linkfile.legacy
LIBS = -lstdc++ -lz -lpspgu -lpspaudio -lpsppower -lm
BUILD_PRX = 0
ENCRYPT = 0
PSP_LARGE_MEMORY = 0
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Elli'\''s House
PSP_EBOOT_ICON = meta/ICON0.PNG
PSP_EBOOT_PIC1 = meta/PIC1.PNG
PSPSDK = $(PSPDEV)/psp/sdk
include $(PSPSDK)/lib/build.mak
# The current SDK defaults to 128-byte ELF segments. Hardware-proven legacy
# homebrew uses the PSP loader's native 4 KiB segment alignment. Hide the
# comma from make's subst argument parser while removing the SDK default.
PSP_COMMA := ,
LDFLAGS := $(subst -Wl$(PSP_COMMA)-zmax-page-size=128,,$(LDFLAGS)) -Wl,-zmax-page-size=4096
source/raylib_psp.o: include/asset_map.hpp include/raylib.h
EBOOT.PBP: meta/ICON0.PNG meta/PIC1.PNG
source/game.o: ../src/game.cpp ../src/game.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
source/manifest.o: ../src/manifest.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
