# Elli's House — ports

Ports of **Elli's House** for Windows (C++), Game Boy Advance, Nintendo 3DS and
PSP. The original game was created in GameMaker by the **Glacial Moon Games
team** for **GameJaaj 7**. Development of these ports was assisted by AI.

The original Windows/GameMaker release is available on
[itch.io](https://glacial-moon-games.itch.io/ellishouse).
That original itch.io version was made by the team without AI assistance; the
AI disclosure in this repository applies only to the later ports.

## Downloads

Ready-to-play packages are published in GitHub Releases:

| Platform | Release file | Contents |
|---|---|---|
| Windows | `EllisHouse-Windows.zip` | `ellis_house.exe` and `assets` |
| Game Boy Advance | `EllisHouse-GBA.gba` | GBA ROM, internal code `ELLI` |
| Nintendo 3DS | `EllisHouse-3DSX.zip` | Homebrew Launcher package |
| Nintendo 3DS | `EllisHouse-3DS.cia` | CIA package |
| PSP | `EllisHouse-PSP.zip` | `PSP/GAME/EllisHousePSP` package |
| Artwork | `EllisHouse-Covers.zip` | EZ-Flash, DS Style and RetroFlow covers |

Installation instructions:

- [Windows](docs/INSTALL_WINDOWS.md)
- [Game Boy Advance and EZ-Flash](docs/INSTALL_GBA.md)
- [Nintendo 3DS](docs/INSTALL_3DS.md)
- [PSP, PSP Go and PS Vita/RetroFlow](docs/INSTALL_PSP.md)
- [Homebrew stores](docs/HOMEBREW_STORES.md)

## Controls

### Windows

- `A/D` or arrows: move
- `Space`: jump
- `W` while moving in the air: dash
- `S`: crouch, slide and interact with signs
- `Esc`: pause
- `R`: restart room

### Nintendo 3DS

- Circle Pad or D-Pad: move; down crouches/slides
- `B` or `X`: jump
- `Y`, `A`, `L` or `R`: dash
- `Start`: pause

### PSP

- D-Pad or analog: move; down crouches/slides
- Cross or Square: jump
- Circle, Triangle, `L` or `R`: dash
- `Start`: pause

## Build from source

The shared runtime is in `src/`; platform backends are in `gba/`, `3ds/` and
`psp/`. Converted original data used by the desktop build is in `assets/`.

### Windows

Requires CMake 3.24+, a C++20 compiler and Git. The first configure downloads
raylib 5.5.

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

### GBA

Requires devkitARM and Python 3.

```sh
cd gba
make assets
make -j4
```

### Nintendo 3DS

Requires devkitPro/devkitARM, `3ds-dev`, Citro2D and Citro3D. Run
`3ds/tools/generate_assets.ps1`, then `make -C 3ds -j4`. CIA creation also
requires `bannertool`, `makerom` and `3dstool`; their paths can be passed as
Make variables.

### PSP

Requires the PSPDEV toolchain, GNU Make and Python 3. See
[psp/README.md](psp/README.md). `psp/tools/build.ps1` accepts `PSPDEV`,
`PYTHON` and `BASH` environment overrides.

## Credits and AI disclosure

- Original game and direction: Glacial Moon Games
- Programming: Annie
- Graphics: Pavão Gripado and IGustaMe
- Music: BainoLOL
- Original engine/event: GameMaker, GameJaaj 7
- C++ and console ports: developed with AI assistance

See [ATTRIBUTION.md](ATTRIBUTION.md) for the complete public attribution text.

## Rights

No open-source license has been granted for the game, its assets or these ports.
Unless the respective authors state otherwise, all rights remain reserved.
Public visibility does not grant permission to redistribute or reuse its assets.

