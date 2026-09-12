"""Lossless PSP texture chunks, PCM audio and XMB art from the PC assets."""
from pathlib import Path
import math, shutil, struct, zlib
import numpy as np
import soundfile as sf
from PIL import Image

port=Path(__file__).resolve().parents[1]
root=port.parent
out=port/'package/ASSETS'
out.mkdir(parents=True,exist_ok=True)
(port/'meta').mkdir(exist_ok=True)
lines=(root/'assets/game.manifest').read_text(encoding='utf-8').splitlines()
paths=set()
sprites={}
sounds=[]
for line in lines:
    f=line.split('\t')
    if f[0]=='SPRITE':
        sprites[f[1]]=f[12].split(';')
        paths.update(p for p in sprites[f[1]] if p)
    elif f[0]=='FONT': paths.add(f[2])
    elif f[0]=='SOUND': sounds.append(f[2])
shutil.copyfile(root/'assets/game.manifest',out/'GAME.MANIFEST')
assets=[]
parts=[]
with (out/'TEX.BIN').open('wb') as tex,(out/'MASK.BIN').open('wb') as masks:
    for path in sorted(paths):
        im=Image.open(root/path).convert('RGBA')
        first=len(parts)
        mask=zlib.compress(im.getchannel('A').tobytes(),1)
        maskoff=masks.tell(); masks.write(mask)
        for y in range(0,im.height,256):
            for x in range(0,im.width,256):
                w=min(256,im.width-x); h=min(256,im.height-y)
                tw=max(16,1<<(w-1).bit_length()); th=max(8,1<<(h-1).bit_length())
                tile=Image.new('RGBA',(tw,th)); tile.paste(im.crop((x,y,x+w,y+h)),(0,0))
                raw=zlib.compress(tile.tobytes(),1)
                parts.append((x,y,w,h,tw,th,tex.tell(),len(raw)))
                tex.write(raw)
        assets.append((path,im.width,im.height,first,len(parts)-first,maskoff,len(mask)))
header=['#pragma once','#include <cstddef>',
    'struct PspAsset { const char* path; int width,height,first,count; unsigned maskOffset,maskSize; };',
    'struct PspPart { int x,y,width,height,tw,th; unsigned offset,size; };',
    'inline const PspAsset pspAssets[] = {']
header += ['{"'+p+'",'+','.join(map(str,v))+'},' for p,*v in assets]
header += ['};','inline const PspPart pspParts[] = {']
header += ['{'+','.join(map(str,p))+'},' for p in parts]
header += ['};']
(port/'include/asset_map.hpp').write_text('\n'.join(header)+'\n')
for path in sounds:
    samples,rate=sf.read(root/path,dtype='float32',always_2d=True)
    if samples.shape[1]==1: samples=np.repeat(samples,2,axis=1)
    samples=samples[:,:2]
    if rate!=44100:
        positions=np.arange(round(len(samples)*44100/rate))*rate/44100
        samples=np.column_stack([np.interp(positions,np.arange(len(samples)),samples[:,c]) for c in range(2)])
    pcm=np.clip(np.rint(samples*32767),-32768,32767).astype('<i2')
    pcm.tofile(out/(Path(path).stem.upper()+'.PCM'))
# The icon is the same artwork as the 3DS, without rescaling its source pixels.
icon=Image.open(root/'3ds/meta/icon.png').convert('RGBA')
icon.save(port/'meta/ICON0.PNG')
bg=Image.open(root/sprites['bg_menu2'][0]).convert('RGBA')
title=Image.open(root/sprites['spr_title'][0]).convert('RGBA')
pic=Image.new('RGBA',(480,272))
for y in range(0,272,bg.height):
    for x in range(0,480,bg.width): pic.alpha_composite(bg,(x,y))
pic.alpha_composite(title,((480-title.width)//2,(272-title.height)//2))
pic.save(port/'meta/PIC1.PNG')
print(f'Prepared {len(assets)} textures / {len(parts)} lossless chunks, {len(sounds)} stereo PCM tracks.')
