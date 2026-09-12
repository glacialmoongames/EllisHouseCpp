#include "platform_keyboard.hpp"

#if defined(_WIN32) && !defined(__3DS__)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void activateAbnt2KeyboardLayout() {
#if defined(_WIN32) && !defined(__3DS__)
    // Portuguese (Brazil ABNT2), activated only for the game process.
    if (const HKL abnt2=LoadKeyboardLayoutW(L"00010416",KLF_ACTIVATE|KLF_SETFORPROCESS))
        ActivateKeyboardLayout(abnt2,KLF_SETFORPROCESS);
#endif
}
