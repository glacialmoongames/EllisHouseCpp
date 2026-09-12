#define KEY_A CTR_KEY_A
#define KEY_R CTR_KEY_R
#define KEY_UP CTR_KEY_UP
#define KEY_DOWN CTR_KEY_DOWN
#define KEY_LEFT CTR_KEY_LEFT
#define KEY_RIGHT CTR_KEY_RIGHT
#include <3ds.h>
#undef KEY_A
#undef KEY_R
#undef KEY_UP
#undef KEY_DOWN
#undef KEY_LEFT
#undef KEY_RIGHT
#include "game.hpp"
#include "manifest.hpp"
#include <cstdio>
#include <filesystem>

// The MP3 and Vorbis decoders use sizeable frame-local work buffers. libctru's
// default 32 KiB application stack is insufficient and corrupts LR while the
// jump sound is preloaded, producing a NoExecuteFault at pc=0.  Keep this as a
// strong symbol so the 3DS CRT reserves enough stack before main starts.
extern "C" std::uint32_t __stacksize__ = 512 * 1024;

int main() {
    romfsInit();
    try {
        auto data=loadManifest(std::filesystem::path("romfs:/assets/game.manifest"));
        Game game(std::move(data),std::filesystem::path("romfs:/"));
        game.run();
    } catch (const std::exception& e) {
        gfxInitDefault(); consoleInit(GFX_BOTTOM,nullptr);
        std::printf("Elli's House 3DS\n\n%s\n\nSTART: sair",e.what());
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown()&KEY_START) break; gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank(); }
        gfxExit();
    }
    romfsExit();
    return 0;
}
