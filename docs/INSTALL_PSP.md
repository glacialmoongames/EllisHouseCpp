# Install on PSP, PSP Go and PS Vita

The package is EBOOT-only; no ISO build is distributed. A homebrew-capable PSP
or a PS Vita with Adrenaline is required.

## PSP-1000/2000/3000 or PSP Go with M2/Memory Stick

1. Download and extract `EllisHouse-PSP.zip`.
2. Copy the complete `EllisHousePSP` folder to
   `ms0:/PSP/GAME/EllisHousePSP`.
3. Confirm that `EBOOT.PBP` and `ASSETS` are directly inside that folder.
4. Open Game > Memory Stick and launch **Elli's House**.

## PSP Go internal storage

Follow the same procedure but copy the folder to
`ef0:/PSP/GAME/EllisHousePSP`.

If ARK reports `80010002`, test with Game Categories Lite disabled first. The
game follows the standard `/PSP/GAME/<folder>/EBOOT.PBP` layout; category
plugins can alter path resolution.

## PS Vita with Adrenaline and RetroFlow Launcher

1. Copy the game to `ux0:/pspemu/PSP/GAME/EllisHousePSP` (or the equivalent
   configured Adrenaline partition).
2. Copy `artwork/psp/retroflow/EllisHousePSP.png` to
   `ux0:/data/RetroFlow/COVERS/PSP/EllisHousePSP.png`.
3. In RetroFlow, select the same Adrenaline partition and run **Rescan**.

RetroFlow matches PSP homebrew artwork to its parent folder name, hence the
exact filename `EllisHousePSP.png`. The included PNG is 250×320. Other
launchers that accept portrait PNG covers can use the included aliases.
