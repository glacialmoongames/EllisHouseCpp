"""Validate packed pixels and masks against PC assets without running the game."""
from pathlib import Path
import re,zlib
from PIL import Image
port=Path(__file__).resolve().parents[1]
root=port.parent
header=(port/'include/asset_map.hpp').read_text()
asset_text,part_text=header.split('inline const PspPart pspParts[] = {')
assets=re.findall(r'\{"([^"]+)",([0-9,]+)\}',asset_text)
parts=[list(map(int,s.split(','))) for s in re.findall(r'\{([0-9,]+)\}',part_text)]
texture_data=(port/'package/ASSETS/TEX.BIN').read_bytes()
mask_data=(port/'package/ASSETS/MASK.BIN').read_bytes()
for name,fields in assets:
    w,h,first,count,mo,ms=map(int,fields.split(','))
    original=Image.open(root/name).convert('RGBA')
    assert original.size==(w,h),name
    reconstructed=Image.new('RGBA',(w,h))
    for x,y,pw,ph,tw,th,offset,size in parts[first:first+count]:
        tile=Image.frombytes('RGBA',(tw,th),zlib.decompress(texture_data[offset:offset+size]))
        reconstructed.paste(tile.crop((0,0,pw,ph)),(x,y))
    assert reconstructed.tobytes()==original.tobytes(),name
    assert zlib.decompress(mask_data[mo:mo+ms])==original.getchannel('A').tobytes(),name
assert (port/'package/ASSETS/GAME.MANIFEST').read_bytes()==(root/'assets/game.manifest').read_bytes()
assert Image.open(port/'meta/ICON0.PNG').tobytes()==Image.open(root/'3ds/meta/icon.png').tobytes()
print(f'PASS: {len(assets)} textures and alpha masks match the PC pixel-for-pixel; manifest and 3DS icon match.')
