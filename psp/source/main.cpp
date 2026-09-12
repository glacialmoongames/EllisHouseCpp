#include <pspkernel.h>
#include <psppower.h>
#include <pspdebug.h>
#include "game.hpp"
#include "manifest.hpp"
#include <filesystem>
#include <cstdio>
#include <sys/stat.h>

// Keep the loader-facing module identifier conservative. The apostrophe is
// retained in PARAM.SFO (the visible XMB title), not in the PRX module name.
// The C++ PSPSDK macro stringifies this argument. Passing a string literal
// embeds the quote characters in the module name and real hardware rejects it.
PSP_MODULE_INFO(EllisHouse,PSP_MODULE_USER,1,0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
PSP_HEAP_SIZE_KB(-2048);

int main(int argc,char** argv) {
    scePowerSetClockFrequency(333,333,166);
    try {
        auto root=std::filesystem::path(argc>0?argv[0]:"EBOOT.PBP").parent_path();
        if(!std::filesystem::exists(root/"ASSETS/GAME.MANIFEST"))root="disc0:/PSP_GAME/USRDIR";
        auto data=loadManifest(root/"ASSETS/GAME.MANIFEST");
        Game game(std::move(data),root);
        game.run();
    } catch(const std::exception& error) {
        pspDebugScreenInit();
        pspDebugScreenPrintf("Elli's House\n%s",error.what());
        sceKernelDelayThread(8000000);
    }
    sceKernelExitGame();
    return 0;
}
