#pragma once
// Share only the portable API declarations, never the 3DS implementation.
#include "../../3ds/include/raylib.h"

// PSP display mode exposed to the shared pause menu.
bool IsWindowFullscreen();
