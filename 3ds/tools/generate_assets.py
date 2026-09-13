"""Generate Nintendo 3DS texture sheets, masks, ROMFS and native PCM."""
from pathlib import Path
import shutil
import sys

import numpy as np
from PIL import Image
import soundfile as sf

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
port = root / "3ds"
gfx = port / "gfx"
src = port / "gfx_src"
rom = port / "romfs"
for directory in (gfx, src, rom / "assets", rom / "masks", rom / "audio"):
    directory.mkdir(parents=True, exist_ok=True)

manifest = (root / "assets" / "game.manifest").read_text(encoding="utf-8").splitlines()
shutil.copyfile(root / "assets" / "game.manifest", rom / "assets" / "game.manifest")

paths = set()
sprite_frames = {}
graphic_sprites = set()
sound_paths = []
font_paths = set()
for line in manifest:
    fields = line.split("\t")
    if fields[0] == "SPRITE" and len(fields) > 12:
        frames = [p for p in fields[12].split(";") if p]
        sprite_frames[fields[1]] = frames
        paths.update(frames)
    elif fields[0] == "FONT" and len(fields) > 2:
        paths.add(fields[2])
        font_paths.add(fields[2])
    elif fields[0] == "GRAPHIC" and len(fields) > 2:
        graphic_sprites.add(fields[2])
    elif fields[0] == "SOUND" and len(fields) > 2:
        sound_paths.append(fields[2])

crop_paths = set(font_paths)
for name in graphic_sprites:
    crop_paths.update(sprite_frames.get(name, ()))

assets = []
parts = []
batches = []
batch_images = []
batch_area = 0
batch_index = 0

def flush_batch():
    global batch_images, batch_area, batch_index
    if batch_images:
        batches.append(batch_images)
        batch_images = []
        batch_area = 0
        batch_index += 1

for relative in sorted(paths, key=str.casefold):
    image = Image.open(root / relative).convert("RGBA")
    width, height = image.size
    first = len(parts)
    dedicated = relative in crop_paths
    tiles = [(x, y, min(512, width-x), min(512, height-y))
             for y in range(0, height, 512) for x in range(0, width, 512)]
    for x, y, tile_w, tile_h in tiles:
        area = (tile_w + 4) * (tile_h + 4)
        if (dedicated and batch_images) or len(batch_images) >= 96 or batch_area + area > 700000:
            flush_batch()
        image_id = f"i{len(parts):04d}"
        image.crop((x, y, x + tile_w, y + tile_h)).save(src / f"{image_id}.png")
        parts.append((batch_index, len(batch_images), x, y, tile_w, tile_h))
        batch_images.append(f"../gfx_src/{image_id}.png")
        batch_area += area
        if dedicated:
            flush_batch()
    (rom / "masks" / f"m{len(assets):04d}.a8").write_bytes(image.getchannel("A").tobytes())
    assets.append((relative.replace("\\", "/"), width, height, first, len(tiles)))
flush_batch()

for old in gfx.glob("*.t3s"):
    old.unlink()
for index, images in enumerate(batches):
    options = "--atlas -f rgba5551 -z auto" if len(images) > 1 else "-f rgba5551 -z auto"
    (gfx / f"sheet{index:03d}.t3s").write_text(options + "\n" + "\n".join(images) + "\n", encoding="ascii")

lines = ['#include "texture_map.hpp"', "const TextureAsset3DS gTextureAssets3DS[] = {"]
for relative, width, height, first, count in assets:
    lines.append(f'  {{"romfs:/{relative}",{width},{height},{first},{count}}},')
lines += ["};", "const std::size_t gTextureAssetCount3DS = sizeof(gTextureAssets3DS)/sizeof(gTextureAssets3DS[0]);",
          "const TexturePart3DS gTextureParts3DS[] = {"]
for sheet, image_index, x, y, width, height in parts:
    lines.append(f"  {{{sheet},{image_index},{x},{y},{width},{height}}},")
lines += ["};", "const char* const gTextureSheets3DS[] = {"]
for index in range(len(batches)):
    lines.append(f'  "romfs:/gfx/sheet{index:03d}.t3x",')
lines.append("};")
(port / "source" / "texture_map.cpp").write_text("\n".join(lines) + "\n", encoding="utf-8")

for relative in sound_paths:
    source = root / relative
    samples, rate = sf.read(source, dtype="float32", always_2d=True)
    mono = samples.mean(axis=1)
    if rate != 22050:
        positions = np.arange(round(len(mono) * 22050 / rate), dtype=np.float64) * rate / 22050
        mono = np.interp(positions, np.arange(len(mono)), mono)
    pcm = np.clip(np.rint(mono * 32767), -32768, 32767).astype("<i2")
    pcm.tofile(rom / "audio" / (source.stem.upper() + ".PCM"))

print(f"Generated {len(assets)} textures, {len(parts)} parts, {len(batches)} sheets and {len(sound_paths)} audio tracks.")
