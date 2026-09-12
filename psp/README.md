# Elli's House — PSP

Native PSP port of the shared PC game code. The PSP uses a 384×218 viewport
centered at (48,27) on its 480×272 display, at 1:1 pixel scale. Menus, HUD,
credits, signs, endings, boss, music and New Game Plus use the PC implementation.

## Controls

- D-pad / analog: move; down also crouches, slides and reads signs.
- Cross / square: jump.
- Circle / triangle / L / R: dash while moving in the air.
- Start: pause.
- Cross / circle: confirm menu selection.
- The pause menu display option alternates between pixel-perfect 384×218 and
  proportional full screen (approximately 479×272).

## Output

- `../EllisHousePSP/`: copy the entire folder (EBOOT.PBP and ASSETS) into
  `PSP/GAME`. Use that same folder name on PSP Go internal storage
  (`ef0:/PSP/GAME/EllisHousePSP`), a PSP Go M2 card, or a PSP-3000 Memory
  Stick (`ms0:/PSP/GAME/EllisHousePSP`). Its EBOOT contains a normal static
  PSP ELF with the same single 4 KiB-aligned RWX load segment and `0x08900000`
  base address used by the working C++ Celeste port.
  The SFO leaves the PSP Go `EF2` launch path intact instead of forcing ARK
  to reinterpret an `ef0:` launch as `MS2`.
- Game Categories Lite may place `EllisHousePSP` one level below a category.
  In Folder mode, the plugin itself limits `category/EllisHousePSP` to 30
  characters, excluding the slash. Because `EllisHousePSP` uses 13, the
  category directory component may use at most 17 characters, counting a
  `CAT_` prefix when present. Contextual Menu mode does not have that
  Folder-mode limit.
- Saves use `ef0:/PSP/SAVEDATA/ELLISHOUSE/Save.sav` when launched from PSP Go
  internal storage, or the equivalent `ms0:` path on a Memory Stick.

The packages target a PSP with homebrew/CFW support. Booting from `/PSP/GAME`
has been confirmed on real hardware after disabling an incompatible Game
Categories Lite setup. The latest performance and clipping changes still need
device validation.

Only the `PSP/GAME` package is built. ISO generation is intentionally disabled.

## Rendering and audio

The native GU renderer retains full RGBA8888 colors and alpha, nearest-neighbor
sampling, original pivots, flipped sprites, draw order and PC lighting blend.
Pixel-perfect mode clips the complete camera viewport and clears the remaining
48-pixel side and 27-pixel top/bottom borders to opaque black every frame.
All 783 texture frames and their masks are packed losslessly into 256px chunks;
only needed chunks are decompressed into an 8 MiB RAM cache. Texture eviction
waits until earlier GPU work has completed. Static graphics use the spatial
index shared with 3DS. No enemy activation or simulation is culled by camera.

Music streams as 44.1 kHz stereo PCM in a separate audio thread; sound effects
remain resident. Conversion happens during asset generation, avoiding runtime
MP3/Vorbis decoding and full-song allocations. The main CPU is set to 333 MHz.
The build does not request the additional RAM of PSP-2000/3000.

## Build on this workstation

Run `tools/build.ps1`; add `-PrepareAssets` after changing source assets.
The local SDK is in `work/toolchain/pspdev` and Python dependencies are in
`work/python-psp`, under the workspace. The SDK is PSPDEV GCC 15.2.0 via
https://github.com/dmang-dev/pspdev-win/releases/tag/v2.
`tools/verify_assets.py` compares every packed texture and collision alpha mask
against PC files without executing the application.
