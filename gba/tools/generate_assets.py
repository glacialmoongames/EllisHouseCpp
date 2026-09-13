"""Convert the exported GameMaker manifest to GBA-native, ROM-streamable assets.

Static room layers are flattened once on the host. The GBA consequently performs
only 160 small DMA copies per frame and never scales gameplay art.
"""
from pathlib import Path
from PIL import Image, ImageOps, ImageChops
import struct
import re
import shutil
import subprocess

GBA = Path(__file__).resolve().parents[1]
PROJECT = GBA.parent
MANIFEST = PROJECT / "assets/game.manifest"
OUT = GBA / "generated"
OUT.mkdir(exist_ok=True)

sprites, objects, rooms, sounds, font = {}, {}, [], {}, {"glyphs":{}}
current = None
for raw in MANIFEST.read_text(encoding="utf-8").splitlines():
    p = raw.split("\t")
    if not p:
        continue
    if p[0] == "SPRITE":
        sprites[p[1]] = dict(w=int(p[2]), h=int(p[3]), ox=int(p[4]), oy=int(p[5]),
                             paths=p[12].split(";") if len(p) > 12 else [])
    elif p[0] == "FONT":
        font.update(path=p[2], size=float(p[3]))
    elif p[0] == "GLYPH":
        font["glyphs"][int(p[1])] = dict(x=int(p[2]),y=int(p[3]),w=int(p[4]),h=int(p[5]),offset=int(p[6]),advance=int(p[7]))
    elif p[0] == "SOUND":
        sounds[p[1]]=dict(path=p[2],volume=float(p[3]))
    elif p[0] == "OBJECT":
        objects[p[1]] = dict(sprite=p[2], parent=p[3], visible=p[4] == "1",
                             behavior=p[7] if len(p) > 7 else "none",
                             spawn=p[8] if len(p) > 8 else "",
                             state=int(float(p[9])) if len(p) > 9 and p[9] else 0,
                             timer=int(float(p[10])) if len(p) > 10 and p[10] else 0,
                             type=int(float(p[11])) if len(p) > 11 and p[11] else 0,
                             range=float(p[12]) if len(p) > 12 and p[12] else 0,
                             speed=float(p[13]) if len(p) > 13 and p[13] else 0,
                             vertical_speed=float(p[14]) if len(p) > 14 and p[14] else 0,
                             direction=float(p[15]) if len(p) > 15 and p[15] else 0,
                             spawn_rate=int(float(p[16])) if len(p) > 16 and p[16] else 0,
                             animation_speed=float(p[17]) if len(p) > 17 and p[17] else -1)
    elif p[0] == "ROOM":
        current = dict(name=p[1], w=int(p[2]), h=int(p[3]), startx=int(float(p[7])),
                       starty=int(float(p[8])), instances=[], graphics=[], backgrounds=[])
        rooms.append(current)
    elif current and p[0] == "INSTANCE":
        current["instances"].append(dict(depth=int(p[1]), obj=p[3], x=float(p[4]), y=float(p[5]),
            sx=float(p[6]), sy=float(p[7]), rot=float(p[8]), frame=int(float(p[10]))))
    elif current and p[0] == "GRAPHIC":
        current["graphics"].append(dict(depth=int(p[1]), sprite=p[2], x=float(p[3]), y=float(p[4]),
            l=int(p[5]), t=int(p[6]), w=int(p[7]), h=int(p[8]), sx=float(p[9]), sy=float(p[10]), rot=float(p[11])))
    elif current and p[0] == "BACKGROUND":
        current["backgrounds"].append(dict(depth=int(p[1]), sprite=p[2], colour=int(p[3]),
            tilex=p[5] == "1", tiley=p[6] == "1"))
    elif p[0] == "ENDROOM":
        current = None

rooms = [r for r in rooms if r["name"] in {"rm_0","rm_1","rm_2","rm_3","rm_4","rm_boss"}]

def rgba(sprite, frame=0):
    s = sprites[sprite]
    path = PROJECT / s["paths"][frame % len(s["paths"])]
    return Image.open(path).convert("RGBA")

def transformed(im, sx, sy, rot=0):
    if sx < 0: im = ImageOps.mirror(im)
    if sy < 0: im = ImageOps.flip(im)
    nw, nh = max(1, round(im.width*abs(sx))), max(1, round(im.height*abs(sy)))
    if (nw, nh) != im.size: im = im.resize((nw,nh), Image.Resampling.NEAREST)
    if rot: im = im.rotate(-rot, Image.Resampling.NEAREST, expand=True)
    return im

def paste_instance(dst, inst, mask_only=False):
    obj = objects.get(inst["obj"], {})
    sn = obj.get("sprite", "")
    if not sn or sn not in sprites: return
    s = sprites[sn]
    im = rgba(sn, inst["frame"])
    if mask_only:
        a = im.getchannel("A").point(lambda v: 255 if v else 0)
        im = Image.merge("RGBA", (a,a,a,a))
    im = transformed(im, inst["sx"], inst["sy"], inst["rot"])
    # Rotated collision instances are uncommon; for all normal instances this
    # is the exact GameMaker origin and integer pixel placement.
    x = round(inst["x"] - (s["w"]-s["ox"] if inst["sx"] < 0 else s["ox"])*abs(inst["sx"]))
    y = round(inst["y"] - (s["h"]-s["oy"] if inst["sy"] < 0 else s["oy"])*abs(inst["sy"]))
    dst.alpha_composite(im, (x,y))

def descends(name, base):
    seen=set()
    while name and name not in seen:
        if name == base: return True
        seen.add(name); name=objects.get(name,{}).get("parent","")
    return False

room_pixels = bytearray()
room_pixels_odd = bytearray()
collision_bits = bytearray()
platform_bits = bytearray()
hazard_bits = bytearray()
slope_bits = bytearray()
palettes = bytearray()
metadata = []
signs = []
pickups = []
enemies = []
enemy_sprites = []
enemy_pixels = bytearray()
enemy_spans = bytearray()
boss_sprite_ids=[]
boss_palette_image=None
pickup_names={"obj_chocolate":0,"obj_sock":1,"obj_lollipop":2,"obj_pocket":3}
plus_pickup_names={"obj_bible","obj_cruz","obj_oil","obj_vela","obj_water"}
boss_patch_pixels=bytearray();boss_patch_pixels_odd=bytearray();boss_patch_meta=[];boss_patch_first=[]
blink_sprite_ids=[];bossbar_sprite_id=0xffff
kitchen_sprite_ids={}
behavior_ids={"eslide":1,"eslide_boss":2,"efall":3,"efollow":4,"ethrow":5,
              "ethrow_range":6,"espawner":7,"trash_ball":8,"eye_left":9,
              "eye_right":10,"boss_car":11}
popup_source=rgba("spr_popup").crop((6,0,246,77))
popup_colours=[]
for pixel in popup_source.getdata():
    colour=pixel[:3]
    if colour not in popup_colours: popup_colours.append(colour)
assert len(popup_colours)<=7

for room in rooms:
    w,h=room["w"],room["h"]
    canvas=Image.new("RGBA",(w,h),(0,0,0,255))
    layers=[]
    for bg in room["backgrounds"]: layers.append((bg["depth"],"bg",bg))
    for g in room["graphics"]: layers.append((g["depth"],"graphic",g))
    for i in room["instances"]:
        o=objects.get(i["obj"],{})
        dynamic=o.get("behavior","none")!="none" or i["obj"]=="obj_boss"
        if o.get("visible") and o.get("sprite") and not dynamic and i["obj"] not in {"obj_player","obj_player_cutscene",*pickup_names,*plus_pickup_names}:
            layers.append((i["depth"],"instance",i))
    for _,kind,item in sorted(layers,key=lambda q:q[0],reverse=True):
        if kind == "bg":
            if not item["sprite"]: continue
            im=rgba(item["sprite"])
            for yy in range(0,h,im.height if item["tiley"] else h+1):
                for xx in range(0,w,im.width if item["tilex"] else w+1): canvas.alpha_composite(im,(xx,yy))
        elif kind == "graphic":
            src=rgba(item["sprite"])
            l,t,ww,hh=item["l"],item["t"],item["w"],item["h"]
            x0,x1=sorted((l,l+ww)); y0,y1=sorted((t,t+hh))
            im=src.crop((x0,y0,x1,y1))
            if ww<0: im=ImageOps.mirror(im)
            if hh<0: im=ImageOps.flip(im)
            im=transformed(im,item["sx"],item["sy"],item["rot"])
            canvas.alpha_composite(im,(round(item["x"]),round(item["y"])))
        else: paste_instance(canvas,item)

    rgb=canvas.convert("RGB")
    q=rgb.quantize(colors=247,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    p=(q.getpalette() or [])[:247*3]
    p += [0] * (247*3-len(p))
    indices=bytes(v+1 for v in q.tobytes())
    pixel_off=len(room_pixels); room_pixels.extend(indices)
    for yy in range(h):
        row=indices[yy*w:(yy+1)*w]
        room_pixels_odd.extend(row[1:]+row[-1:])
    pal_off=len(palettes)
    palettes.extend(struct.pack("<H",0))
    for n in range(247):
        r,g,b=p[n*3:n*3+3]
        palettes.extend(struct.pack("<H",(r>>3)|((g>>3)<<5)|((b>>3)<<10)))
    for r,g,b in popup_colours:
        palettes.extend(struct.pack("<H",(r>>3)|((g>>3)<<5)|((b>>3)<<10)))
    for _ in range(7-len(popup_colours)): palettes.extend(struct.pack("<H",0))
    palettes.extend(struct.pack("<H",0x7FFF))

    # Moving enemies use Mode 4 software sprites. Frames are mapped to the
    # room palette on the host and remain at their original pixel dimensions.
    room_enemy_start=len(enemies)
    sprite_cache={}
    palette_image=Image.new("P",(1,1)); palette_image.putpalette(p+[0]*(768-len(p)))
    def enemy_sprite_id(name):
        if not name or name not in sprites: return 0xffff
        if name in sprite_cache: return sprite_cache[name]
        s=sprites[name]; offset=len(enemy_pixels);span_offset=len(enemy_spans)
        images=[Image.open(PROJECT/path).convert("RGBA") for path in s["paths"]]
        union=Image.new("L",images[0].size,0)
        for image in images: union=ImageChops.lighter(union,image.getchannel("A"))
        box=union.getbbox() or (0,0,1,1); left,top,right,bottom=box
        for image in images:
            image=image.crop(box)
            quant=image.convert("RGB").quantize(palette=palette_image,dither=Image.Dither.NONE)
            alpha=image.getchannel("A")
            enemy_pixels.extend(0 if a==0 else c+1 for c,a in zip(quant.getdata(),alpha.getdata()))
            for yy in range(image.height):
                row=alpha.crop((0,yy,image.width,yy+1)).getbbox()
                enemy_spans.extend(struct.pack("<HH",row[0] if row else 0xffff,row[2] if row else 0))
        sid=len(enemy_sprites); sprite_cache[name]=sid
        enemy_sprites.append((offset,span_offset,right-left,bottom-top,s["ox"]-left,s["oy"]-top,len(s["paths"])))
        if room["name"]=="rm_2" and name!="spr_blink": kitchen_sprite_ids[name]=sid
        return sid
    for inst in room["instances"]:
        obj=objects.get(inst["obj"],{}); behavior=obj.get("behavior","none")
        if behavior=="none" and inst["obj"]!="obj_boss": continue
        if inst["obj"]=="obj_boss":
            boss_sprite_ids[:]=[enemy_sprite_id(n) for n in ["spr_boss_idle","spr_boss_eyeatk","spr_boss_eyegrown","spr_boss_death","spr_trash_ball","spr_eyel","spr_eyer"]]
            enemies.append((round(inst["x"]*256),round(inst["y"]*256),boss_sprite_ids[0],0xffff,0xffff,
                12,0,0,0,0,0,10,0,0,0,round(inst["sx"]*256),round(inst["sy"]*256),0,0,5,0))
            continue
        spawn=obj.get("spawn","")
        if inst["obj"]=="obj_dish_washer": spawn="obj_mug"
        elif inst["obj"]=="obj_toolbox": spawn="obj_tools"
        spawn_obj=objects.get(spawn,{})
        alt_obj=objects.get("obj_dish",{}) if inst["obj"]=="obj_dish_washer" else {}
        enemies.append((round(inst["x"]*256),round(inst["y"]*256),enemy_sprite_id(obj.get("sprite","")),
            enemy_sprite_id(spawn_obj.get("sprite","")),enemy_sprite_id(alt_obj.get("sprite","")),
            behavior_ids.get(behavior,0),obj.get("state",0),obj.get("timer",0),round(obj.get("direction",0)*256),
            round(obj.get("range",0)),round(obj.get("speed",0)*256),round(obj.get("vertical_speed",0)*256),
            obj.get("spawn_rate",0),round(spawn_obj.get("speed",0)*256),round(alt_obj.get("speed",0)*256),
            round(inst["sx"]*256),round(inst["sy"]*256),0 if obj.get("type",0)==4 else 1,
            (1 if inst["obj"] in {"obj_ball","obj_tire"} else 0)|
            (2 if inst["obj"] in {"obj_paint","obj_paint2"} else 0)|
            (4 if inst["obj"]=="obj_littlepaint" else 0),
            max(1,round(1/obj["animation_speed"])) if obj.get("animation_speed",-1)>0 else 0,0))
    blink_sprite_ids.append(enemy_sprite_id("spr_blink"))
    if room["name"]=="rm_boss": bossbar_sprite_id=enemy_sprite_id("spr_bossbar")
    if room["name"]=="rm_boss": boss_palette_image=palette_image.copy()

    # The boss is a stationary 448x252 sprite. Drawing it pixel-by-pixel was
    # the dominant runtime cost, so cache each animation frame already merged
    # with the room and stream only its small visible world patch by DMA.
    if room["name"]=="rm_boss":
        inst=next(i for i in room["instances"] if i["obj"]=="obj_boss")
        names=["spr_boss_idle","spr_boss_eyeatk","spr_boss_eyegrown","spr_boss_death"]
        boxes=[]
        for name in names:
            s=sprites[name]
            for path in s["paths"]:
                im=Image.open(PROJECT/path).convert("RGBA"); box=im.getchannel("A").getbbox()
                if box: boxes.append((round(inst["x"]-s["ox"])+box[0],round(inst["y"]-s["oy"])+box[1],round(inst["x"]-s["ox"])+box[2],round(inst["y"]-s["oy"])+box[3]))
        # Cache the complete room so this replaces, rather than supplements,
        # the ordinary background DMA pass. This is faster than two clipped
        # transfers and also keeps every uncovered pixel deterministic.
        bx0=by0=0;bx1=w;by1=h
        bw,bh=bx1-bx0,by1-by0
        for name in names:
            boss_patch_first.append(len(boss_patch_meta))
            s=sprites[name]
            for path in s["paths"]:
                merged=canvas.copy();im=Image.open(PROJECT/path).convert("RGBA")
                merged.alpha_composite(im,(round(inst["x"]-s["ox"]),round(inst["y"]-s["oy"])))
                quant=merged.convert("RGB").quantize(palette=palette_image,dither=Image.Dither.NONE).crop((bx0,by0,bx1,by1))
                values=bytes(v+1 for v in quant.tobytes());off=len(boss_patch_pixels);boss_patch_pixels.extend(values)
                for yy in range(bh):
                    row=values[yy*bw:(yy+1)*bw];boss_patch_pixels_odd.extend(row[1:]+row[-1:])
                boss_patch_meta.append((off,bx0,by0,bw,bh))

    solid=Image.new("RGBA",(w,h),(0,0,0,0)); one_way=Image.new("RGBA",(w,h),(0,0,0,0)); hazard=Image.new("RGBA",(w,h),(0,0,0,0)); slope=Image.new("RGBA",(w,h),(0,0,0,0))
    room_sign_start=len(signs); room_pickup_start=len(pickups)
    next_x,next_y,next_w,next_h=w-8,0,8,h
    for inst in room["instances"]:
        if inst["obj"].startswith("obj_placa"):
            signs.append((round(inst["x"]),round(inst["y"]),int(inst["obj"][-1])))
        if inst["obj"] in pickup_names:
            pickups.append((round(inst["x"]),round(inst["y"]),pickup_names[inst["obj"]]))
        if inst["obj"] == "obj_next":
            sn=objects["obj_next"]["sprite"]; ss=sprites[sn]
            next_x=round(inst["x"]-ss["ox"]*abs(inst["sx"])); next_y=round(inst["y"]-ss["oy"]*abs(inst["sy"]))
            next_w=max(1,round(ss["w"]*abs(inst["sx"]))); next_h=max(1,round(ss["h"]*abs(inst["sy"])))
        if descends(inst["obj"],"obj_colision"):
            paste_instance(one_way if inst["obj"]=="obj_platform" else solid,inst,True)
            if inst["obj"]=="obj_slope": paste_instance(slope,inst,True)
        obj=objects.get(inst["obj"],{})
        if inst["obj"] in {"obj_spike","obj_spikeinv"}:
            paste_instance(hazard,inst,True)
    def add_mask(image,target):
        alpha=image.getchannel("A"); packed=bytearray((w*h+7)//8)
        for n,v in enumerate(alpha.getdata()):
            if v: packed[n>>3] |= 1<<(n&7)
        off=len(target); target.extend(packed); return off
    col_off=add_mask(solid,collision_bits); plat_off=add_mask(one_way,platform_bits); hazard_off=add_mask(hazard,hazard_bits)
    slope_off=add_mask(slope,slope_bits) if slope.getchannel("A").getbbox() else 0xffffffff
    metadata.append((w,h,room["startx"],room["starty"],pixel_off,col_off,plat_off,hazard_off,slope_off,pal_off,
                     room_sign_start,len(signs)-room_sign_start,room_pickup_start,len(pickups)-room_pickup_start,
                     room_enemy_start,len(enemies)-room_enemy_start,next_x,next_y,next_w,next_h))
    print(f"{room['name']}: {w}x{h}, {len(room['instances'])} instancias")

# Shared 4bpp OBJ palette and exact 21x26 player frames padded (never resized).
groups=["idle","run","jump","fall","walljump","crouch","slide","dash","blink","death"]
frames=[]; frame_groups=[]
for gi,name in enumerate(groups):
    for path in sprites[f"spr_player_{name}"]["paths"]:
        im=Image.open(PROJECT/path).convert("RGBA")
        pad=Image.new("RGBA",(32,32),(0,0,0,0)); pad.alpha_composite(im,(5,3)); frames.append(pad); frame_groups.append(gi)
strip=Image.new("RGBA",(32*len(frames),32),(0,0,0,0))
for i,im in enumerate(frames): strip.alpha_composite(im,(i*32,0))
opaque=Image.new("RGB",strip.size,(0,0,0)); opaque.paste(strip.convert("RGB"),mask=strip.getchannel("A"))
pq=opaque.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
pp=(pq.getpalette() or [])[:45]; pp += [0]*(45-len(pp))
obj_palette=[0]
for n in range(15):
    r,g,b=pp[n*3:n*3+3]; obj_palette.append((r>>3)|((g>>3)<<5)|((b>>3)<<10))
obj_tiles=bytearray()
for fi,im in enumerate(frames):
    crop=pq.crop((fi*32,0,fi*32+32,32)); pix=list(crop.getdata()); alpha=list(im.getchannel("A").getdata())
    vals=[0 if alpha[n]==0 else pix[n]+1 for n in range(1024)]
    for ty in range(0,32,8):
        for tx in range(0,32,8):
            for y in range(8):
                for x in range(0,8,2):
                    a=vals[(ty+y)*32+tx+x]; b=vals[(ty+y)*32+tx+x+1]; obj_tiles.append(a|(b<<4))

# Native dash/slide trail frames mapped to the existing player OBJ palette.
# Only one 32x32 frame is uploaded at a time, so the effect costs no permanent
# extra VRAM and remains safe beside the staircase's hardware paintings.
player_colours=[tuple(pp[n*3:n*3+3]) for n in range(15)]
trail_tiles=bytearray()
for name in ("spr_trail","spr_trailslide"):
    for path in sprites[name]["paths"]:
        source=Image.open(PROJECT/path).convert("RGBA")
        cell=Image.new("RGBA",(32,32),(0,0,0,0));cell.alpha_composite(source,(5,3))
        values=[]
        for rr,gg,bb,aa in cell.getdata():
            if not aa: values.append(0);continue
            nearest=min(range(15),key=lambda n:(rr-player_colours[n][0])**2+(gg-player_colours[n][1])**2+(bb-player_colours[n][2])**2)
            values.append(nearest+1)
        for ty in range(0,32,8):
            for tx in range(0,32,8):
                for y in range(8):
                    for x in range(0,8,2):
                        trail_tiles.append(values[(ty+y)*32+tx+x]|(values[(ty+y)*32+tx+x+1]<<4))

# The original font is rasterized at the exact 0.5 scale used by Draw GUI.
font_image=Image.open(PROJECT/font["path"]).convert("RGBA")
def draw_project_text(target,text,center_x,y):
    lines=text.split("\n")
    for line in lines:
        width=sum(font["glyphs"].get(ord(ch),{"advance":16})["advance"]*.5 for ch in line)
        pen=center_x-width/2
        for ch in line:
            glyph=font["glyphs"].get(ord(ch))
            if glyph:
                mask=font_image.crop((glyph["x"],glyph["y"],glyph["x"]+glyph["w"],glyph["y"]+glyph["h"])).getchannel("A")
                mask=mask.resize((max(1,round(glyph["w"]*.5)),max(1,round(glyph["h"]*.5))),Image.Resampling.NEAREST)
                ink=Image.new("RGBA",mask.size,(0,0,0,0)); ink.putalpha(mask)
                target.alpha_composite(ink,(round(pen+glyph["offset"]*.5),round(y)))
                pen+=glyph["advance"]*.5
            else: pen+=8
        y+=8

def draw_menu_text(target,text,center_x,center_y,scale=.75):
    width=sum(font["glyphs"].get(ord(ch),{"advance":16})["advance"]*scale for ch in text)
    pen=center_x-width/2
    for ch in text:
        glyph=font["glyphs"].get(ord(ch))
        if not glyph: pen+=8*scale;continue
        mask=font_image.crop((glyph["x"],glyph["y"],glyph["x"]+glyph["w"],glyph["y"]+glyph["h"])).getchannel("A")
        mask=mask.resize((max(1,round(glyph["w"]*scale)),max(1,round(glyph["h"]*scale))),Image.Resampling.NEAREST)
        ink=Image.new("RGBA",mask.size,(255,255,255,0));ink.putalpha(mask)
        target.alpha_composite(ink,(round(pen+glyph["offset"]*scale),round(center_y-mask.height/2)))
        pen+=glyph["advance"]*scale

# Minimal GBA title screen: the original title is kept at native resolution,
# centred on the LCD, with only the localized play label near the bottom.
menu_frames=[]
for label in ("Play",):
    screen=rgba("bg_menu2").crop((72,29,312,189))
    title=rgba("spr_title");ts=sprites["spr_title"]
    screen.alpha_composite(title,(120-ts["ox"],70-ts["oy"]))
    draw_menu_text(screen,label,120,146)
    menu_frames.append(screen.convert("RGB"))
menu_strip=Image.new("RGB",(240,160))
for i,screen in enumerate(menu_frames):menu_strip.paste(screen,(i*240,0))
menu_q=menu_strip.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
menu_p=(menu_q.getpalette() or [])[:768];menu_p += [0]*(768-len(menu_p))
menu_palette=[]
for n in range(256):
    rr,gg,bb=menu_p[n*3:n*3+3];menu_palette.append((rr>>3)|((gg>>3)<<5)|((bb>>3)<<10))
menu_pixels=bytearray()
menu_pixels.extend(menu_q.crop((0,0,240,160)).tobytes())

# Pause keeps the original localized 177x177 frame at native size.  The lower
# decorative edge is clipped by the 160-line LCD instead of distorting the art.
# Fullscreen is desktop-only, so the GBA menu retains the meaningful actions.
pause_frames=[]
pause_labels=("Resume","Reset","Menu")
for language in range(1):
    panel=rgba("spr_pause",0)
    for selected in range(3):
        screen=Image.new("RGBA",(240,160),(0,0,0,255))
        screen.alpha_composite(panel,(31,0))
        for index,label in enumerate(pause_labels):
            shown=f"> {label} <" if index==selected else label
            draw_menu_text(screen,shown,120,58+index*29,.6)
        pause_frames.append(screen.convert("RGB"))
pause_strip=Image.new("RGB",(240*len(pause_frames),160))
for i,screen in enumerate(pause_frames):pause_strip.paste(screen,(i*240,0))
pause_q=pause_strip.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
pause_p=(pause_q.getpalette() or [])[:768];pause_p += [0]*(768-len(pause_p))
pause_palette=[]
for n in range(256):
    rr,gg,bb=pause_p[n*3:n*3+3];pause_palette.append((rr>>3)|((gg>>3)<<5)|((bb>>3)<<10))
pause_pixels=bytearray()
for i in range(len(pause_frames)):pause_pixels.extend(pause_q.crop((i*240,0,i*240+240,160)).tobytes())

# Room titles are three 64x64 8bpp hardware OBJs. Alpha blending is performed
# by the GBA, replacing the expensive per-pixel software fade used previously.
title_suffixes=["bedroom","stairs","kitchen","living","corridor"]
title_images=[];title_base_meta=[]
for suffix in title_suffixes:
    for name in ("spr_en_"+suffix,):
        source=rgba(name);box=source.getchannel("A").getbbox() or (0,0,1,1)
        left,top,right,bottom=box
        if right-left>192 or bottom-top>72:raise RuntimeError(f"titulo {name} excede 192x72")
        cell=Image.new("RGBA",(192,72),(0,0,0,0));cell.alpha_composite(source.crop(box),(0,0))
        overflow=cell.getchannel("A").crop((0,64,192,72)).getbbox()
        bottom_x=(overflow[0]//32)*32 if overflow else 0
        bottom_count=((overflow[2]+31)//32-bottom_x//32) if overflow else 0
        title_images.append(cell);title_base_meta.append((left-sprites[name]["ox"],top-sprites[name]["oy"],(right-left+63)//64,bottom_x,bottom_count))
# These title sprites use only their two original solid colours. Store those
# colours directly instead of running them through a shared quantizer, so the
# RGB555 values are deterministic and exactly match the source conversion.
title_colours=[]
for image in title_images:
    for rr,gg,bb,aa in image.getdata():
        colour=(rr,gg,bb)
        if aa and colour not in title_colours:title_colours.append(colour)
if len(title_colours)>159:raise RuntimeError("titulos excedem a paleta OBJ reservada")
title_colour_index={colour:i+1 for i,colour in enumerate(title_colours)}
def rgb555_nearest(rr,gg,bb):
    return (rr*31+127)//255|(((gg*31+127)//255)<<5)|(((bb*31+127)//255)<<10)
title_palette=[0]+[rgb555_nearest(rr,gg,bb) for rr,gg,bb in title_colours]
title_palette += [0]*(160-len(title_palette))
title_tiles=bytearray();title_meta=[]
for i,image in enumerate(title_images):
    values=[0 if aa==0 else title_colour_index[(rr,gg,bb)]+96 for rr,gg,bb,aa in image.getdata()]
    offset=len(title_tiles)
    for block in range(3):
        for ty in range(0,64,8):
            for tx in range(block*64,block*64+64,8):
                for yy in range(8):
                    title_tiles.extend(values[(ty+yy)*192+tx:(ty+yy)*192+tx+8])
    xoff,yoff,blocks,bottom_x,bottom_count=title_base_meta[i]
    for block in range(bottom_count):
        tx0=bottom_x+block*32
        for tx in range(tx0,tx0+32,8):
            for yy in range(64,72):title_tiles.extend(values[yy*192+tx:yy*192+tx+8])
    title_meta.append((offset,len(title_tiles)-offset,xoff,yoff,blocks,bottom_x,bottom_count))

# Read the literals from the preserved GML instead of maintaining a translated
# or duplicated copy. Any byte-level text correction in the source project is
# therefore inherited automatically by the ROM build.
texts_en=[]; texts_pt=[]
for number in range(1,7):
    source=(PROJECT/f"reference/gml/objects/obj_placa{number}/Create_0.gml").read_text(encoding="utf-8")
    blocks=re.findall(r'@"\r?\n(.*?)\r?\n"',source,re.DOTALL)
    if len(blocks)!=2: raise RuntimeError(f"placa {number}: esperado texto PT e EN")
    texts_pt.append(blocks[0].replace("\r\n","\n")); texts_en.append(blocks[1].replace("\r\n","\n"))
dialog_bytes=bytearray(); popup_index={c:248+i for i,c in enumerate(popup_colours)}
for message in texts_en:
    panel=popup_source.copy(); draw_project_text(panel,message,120,1)
    for pixel in panel.getdata():
        colour=pixel[:3]
        dialog_bytes.append(0 if colour==(0,0,0) else popup_index[colour])

# 16x16 prompt: original bubble and original literal "S", also at scale 0.5.
button=Image.new("RGBA",(16,16),(0,0,0,0)); button.alpha_composite(rgba("spr_button"),(2,2))
draw_project_text(button,"S",8.5,4)
b_rgb=Image.new("RGB",button.size,(0,0,0)); b_rgb.paste(button.convert("RGB"),mask=button.getchannel("A"))
bq=b_rgb.quantize(colors=15,dither=Image.Dither.NONE); bp=(bq.getpalette() or [])[:45]; bp += [0]*(45-len(bp))
button_pal=[0]+[((bp[n*3]>>3)|((bp[n*3+1]>>3)<<5)|((bp[n*3+2]>>3)<<10)) for n in range(15)]
button_vals=[0 if a==0 else c+1 for c,a in zip(bq.getdata(),button.getchannel("A").getdata())]
button_tiles=bytearray()
for ty in (0,8):
  for tx in (0,8):
    for y in range(8):
      for x in range(0,8,2): button_tiles.append(button_vals[(ty+y)*16+tx+x]|(button_vals[(ty+y)*16+tx+x+1]<<4))

# Highlighted plaque frame gets a dedicated palette bank. It is overlaid on the
# flattened idle plaque when the player is in range, matching image_index = 1.
sign_im=rgba("spr_placa",1)
s_rgb=Image.new("RGB",sign_im.size,(0,0,0)); s_rgb.paste(sign_im.convert("RGB"),mask=sign_im.getchannel("A"))
sq=s_rgb.quantize(colors=15,dither=Image.Dither.NONE); sp=(sq.getpalette() or [])[:45]; sp += [0]*(45-len(sp))
sign_pal=[0]+[((sp[n*3]>>3)|((sp[n*3+1]>>3)<<5)|((sp[n*3+2]>>3)<<10)) for n in range(15)]
sign_vals=[0 if a==0 else c+1 for c,a in zip(sq.getdata(),sign_im.getchannel("A").getdata())]
sign_tiles=bytearray()
for ty in (0,8):
  for tx in (0,8):
    for y in range(8):
      for x in range(0,8,2): sign_tiles.append(sign_vals[(ty+y)*16+tx+x]|(sign_vals[(ty+y)*16+tx+x+1]<<4))

# Four collectible types share one OBJ palette and stay at their native 16x16.
pickup_sprite_names=["spr_chocolat","spr_item1","spr_item2","spr_item3"]
pickup_strip=Image.new("RGBA",(64,16),(0,0,0,0))
for i,name in enumerate(pickup_sprite_names): pickup_strip.alpha_composite(rgba(name),(i*16,0))
pickup_rgb=Image.new("RGB",pickup_strip.size,(0,0,0)); pickup_rgb.paste(pickup_strip.convert("RGB"),mask=pickup_strip.getchannel("A"))
pickup_q=pickup_rgb.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
pickup_p=(pickup_q.getpalette() or [])[:45]; pickup_p += [0]*(45-len(pickup_p))
pickup_pal=[0]+[((pickup_p[n*3]>>3)|((pickup_p[n*3+1]>>3)<<5)|((pickup_p[n*3+2]>>3)<<10)) for n in range(15)]
pickup_tiles=bytearray()
for i in range(4):
    image=pickup_strip.crop((i*16,0,i*16+16,16)); quant=pickup_q.crop((i*16,0,i*16+16,16))
    values=[0 if a==0 else c+1 for c,a in zip(quant.getdata(),image.getchannel("A").getdata())]
    for ty in (0,8):
      for tx in (0,8):
        for y in range(8):
          for x in range(0,8,2): pickup_tiles.append(values[(ty+y)*16+tx+x]|(values[(ty+y)*16+tx+x+1]<<4))

# The staircase's repeated paintings use hardware OBJs instead of the software
# blitter. Both sizes and both original frames share palette bank 5.
paint_names=[f"spr_paint{i:02d}" for i in range(1,5)]+[f"spr_lilpaint{i:02d}" for i in (1,2,3,4)]
paint_frames=[rgba(name,frame) for name in paint_names for frame in range(2)]
paint_strip=Image.new("RGBA",(sum(i.width for i in paint_frames),32),(0,0,0,0));paint_x=[];xx=0
for image in paint_frames:paint_x.append(xx);paint_strip.alpha_composite(image,(xx,0));xx+=image.width
paint_rgb=Image.new("RGB",paint_strip.size,(0,0,0));paint_rgb.paste(paint_strip.convert("RGB"),mask=paint_strip.getchannel("A"))
paint_q=paint_rgb.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
paint_p=(paint_q.getpalette() or [])[:45];paint_p += [0]*(45-len(paint_p))
paint_pal=[0]+[((paint_p[n*3]>>3)|((paint_p[n*3+1]>>3)<<5)|((paint_p[n*3+2]>>3)<<10)) for n in range(15)]
paint_tiles=bytearray();paint_frame_tiles=[]
for image,x in zip(paint_frames,paint_x):
    frame_start=len(paint_tiles)
    quant=paint_q.crop((x,0,x+image.width,image.height))
    values=[0 if a==0 else c+1 for c,a in zip(quant.getdata(),image.getchannel("A").getdata())]
    for ty in range(0,image.height,8):
      for tx in range(0,image.width,8):
        for yy in range(8):
          for xx in range(0,8,2):paint_tiles.append(values[(ty+yy)*image.width+tx+xx]|(values[(ty+yy)*image.width+tx+xx+1]<<4))
    paint_frame_tiles.append(bytes(paint_tiles[frame_start:]))
paint_base_tiles=paint_frame_tiles[0]+paint_frame_tiles[1]+paint_frame_tiles[8]+paint_frame_tiles[9]

# Boss HUD: back, ten reusable life segments, and front are hardware OBJs.
bar_front=rgba("spr_bossbar");bar_back=rgba("spr_bossbar_back")
bar_parts=[]
for source in (bar_front,bar_back):
    for x,wpart in ((0,64),(64,64),(128,64),(192,64),(256,16)):
        part=Image.new("RGBA",(wpart,32),(0,0,0,0));part.alpha_composite(source.crop((x,0,min(x+wpart,270),24)),(0,0));bar_parts.append(part)
life_part=Image.new("RGBA",(32,16),(0,0,0,0));life_part.paste((189,85,95,255),(0,0,23,13));bar_parts.append(life_part)
bstrip=Image.new("RGBA",(sum(i.width for i in bar_parts),32),(0,0,0,0));bxs=[];xx=0
for image in bar_parts:bxs.append(xx);bstrip.alpha_composite(image,(xx,0));xx+=image.width
brgb=Image.new("RGB",bstrip.size,(0,0,0));brgb.paste(bstrip.convert("RGB"),mask=bstrip.getchannel("A"))
bq=brgb.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE);bp=(bq.getpalette() or [])[:45];bp += [0]*(45-len(bp))
bossbar_palette=[0]+[((bp[n*3]>>3)|((bp[n*3+1]>>3)<<5)|((bp[n*3+2]>>3)<<10)) for n in range(15)]
bossbar_tiles=bytearray();bossbar_meta=[]
for image,x in zip(bar_parts,bxs):
    first=608+len(bossbar_tiles)//32;quant=bq.crop((x,0,x+image.width,image.height));vals=[0 if a==0 else c+1 for c,a in zip(quant.getdata(),image.getchannel("A").getdata())]
    for ty in range(0,image.height,8):
      for tx in range(0,image.width,8):
        for yy in range(8):
          for px in range(0,8,2):bossbar_tiles.append(vals[(ty+yy)*image.width+tx+px]|(vals[(ty+yy)*image.width+tx+px+1]<<4))
    bossbar_meta.append((first,image.width,image.height))

# Cache all 5-degree trash-ball rotations on the host. Runtime rotation was a
# multiplication-heavy inverse mapping over a large transparent rectangle; the
# precomputed opaque spans keep the original 64x64 art and make the attack as
# cheap as the other unscaled software sprites.
if boss_palette_image is None:raise RuntimeError("paleta da sala do chefe ausente")
trash_source=rgba("spr_trash_ball")
trash_rotated=[trash_source.rotate(angle,Image.Resampling.NEAREST,expand=True) for angle in range(0,360,5)]
trash_rot_w=max(image.width for image in trash_rotated);trash_rot_h=max(image.height for image in trash_rotated)
trash_rot_ox=trash_rot_w//2;trash_rot_oy=trash_rot_h//2
trash_rot_pixels=bytearray();trash_rot_spans=bytearray()
for rotated in trash_rotated:
    cell=Image.new("RGBA",(trash_rot_w,trash_rot_h),(0,0,0,0))
    cell.alpha_composite(rotated,((trash_rot_w-rotated.width)//2,(trash_rot_h-rotated.height)//2))
    quant=cell.convert("RGB").quantize(palette=boss_palette_image,dither=Image.Dither.NONE)
    alpha=cell.getchannel("A");values=bytes(0 if aa==0 else cc+1 for cc,aa in zip(quant.getdata(),alpha.getdata()))
    trash_rot_pixels.extend(values)
    for yy in range(trash_rot_h):
        span=alpha.crop((0,yy,trash_rot_w,yy+1)).getbbox()
        trash_rot_spans.extend(struct.pack("<HH",span[0] if span else 0xffff,span[2] if span else 0))

# Kitchen enemies fit together in the title's reclaimed OBJ tile area. One
# dedicated 4bpp palette and hardware sprites remove their VRAM read/modify/write
# blits entirely while retaining both normal and outlined animation frames.
kitchen_names=sorted(kitchen_sprite_ids,key=lambda name:kitchen_sprite_ids[name])
kitchen_ids=[kitchen_sprite_ids[name] for name in kitchen_names]
if kitchen_ids and kitchen_ids!=list(range(kitchen_ids[0],kitchen_ids[0]+len(kitchen_ids))):
    raise RuntimeError("IDs dos sprites da cozinha deixaram de ser contiguos")
kitchen_cells=[]
for name in kitchen_names:
    s=sprites[name];cw=16 if s["w"]<=16 and s["h"]<=16 else 32;ch=64 if s["h"]>32 else (32 if cw==32 else 16)
    for frame in range(len(s["paths"])):
        cell=Image.new("RGBA",(cw,ch),(0,0,0,0));cell.alpha_composite(rgba(name,frame),(0,0));kitchen_cells.append(cell)
kstrip=Image.new("RGBA",(sum(i.width for i in kitchen_cells),64),(0,0,0,0));kxs=[];xx=0
for image in kitchen_cells:kxs.append(xx);kstrip.alpha_composite(image,(xx,0));xx+=image.width
krgb=Image.new("RGB",kstrip.size,(0,0,0));krgb.paste(kstrip.convert("RGB"),mask=kstrip.getchannel("A"))
kq=krgb.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE);kp=(kq.getpalette() or [])[:45];kp += [0]*(45-len(kp))
kitchen_palette=[0]+[((kp[n*3]>>3)|((kp[n*3+1]>>3)<<5)|((kp[n*3+2]>>3)<<10)) for n in range(15)]
kitchen_tiles=bytearray();kitchen_meta=[];cell_index=0;tile_base=768
for name in kitchen_names:
    s=sprites[name];cw=16 if s["w"]<=16 and s["h"]<=16 else 32;ch=64 if s["h"]>32 else (32 if cw==32 else 16)
    first_tile=tile_base+len(kitchen_tiles)//32;frame_tiles=cw*ch//64
    for frame in range(len(s["paths"])):
        image=kitchen_cells[cell_index];quant=kq.crop((kxs[cell_index],0,kxs[cell_index]+cw,ch));cell_index+=1
        vals=[0 if a==0 else c+1 for c,a in zip(quant.getdata(),image.getchannel("A").getdata())]
        for ty in range(0,ch,8):
          for tx in range(0,cw,8):
            for yy in range(8):
              for x in range(0,8,2):kitchen_tiles.append(vals[(ty+yy)*cw+tx+x]|(vals[(ty+yy)*cw+tx+x+1]<<4))
    kitchen_meta.append((kitchen_sprite_ids[name],first_tile,frame_tiles,cw,ch,s["ox"],s["oy"]))
if tile_base+len(kitchen_tiles)//32>1024:raise RuntimeError("sprites de hardware da cozinha excedem OBJ VRAM")

# Original skull HUD sprite and font0 digits at the HUD's exact 0.75 scale.
skull=rgba("spr_cavera"); skull_rgb=Image.new("RGB",skull.size,(0,0,0)); skull_rgb.paste(skull.convert("RGB"),mask=skull.getchannel("A"))
skull_q=skull_rgb.quantize(colors=15,dither=Image.Dither.NONE); hp=(skull_q.getpalette() or [])[:45]; hp += [0]*(45-len(hp))
skull_pal=[0]+[((hp[n*3]>>3)|((hp[n*3+1]>>3)<<5)|((hp[n*3+2]>>3)<<10)) for n in range(15)]
skull_values=[0 if a==0 else c+1 for c,a in zip(skull_q.getdata(),skull.getchannel("A").getdata())]
skull_tiles=bytearray()
for ty in (0,8):
  for tx in (0,8):
    for y in range(8):
      for x in range(0,8,2): skull_tiles.append(skull_values[(ty+y)*16+tx+x]|(skull_values[(ty+y)*16+tx+x+1]<<4))
hud_digits=bytearray()
for value in range(10):
    glyph=font["glyphs"][ord(str(value))]; mask=font_image.crop((glyph["x"],glyph["y"],glyph["x"]+glyph["w"],glyph["y"]+glyph["h"])).getchannel("A")
    mask=mask.resize((max(1,round(glyph["w"]*.75)),max(1,round(glyph["h"]*.75))),Image.Resampling.NEAREST)
    cell=Image.new("L",(12,12),0); cell.paste(mask,(round(glyph["offset"]*.75),0))
    hud_digits.extend(cell.tobytes())

(OUT/"rooms.bin").write_bytes(room_pixels)
(OUT/"rooms_odd.bin").write_bytes(room_pixels_odd)
(OUT/"collision.bin").write_bytes(collision_bits)
(OUT/"platform.bin").write_bytes(platform_bits)
(OUT/"hazard.bin").write_bytes(hazard_bits)
(OUT/"slope.bin").write_bytes(slope_bits)
(OUT/"palettes.bin").write_bytes(palettes)
(OUT/"player.bin").write_bytes(obj_tiles)
(OUT/"trails.bin").write_bytes(trail_tiles)
(OUT/"objpal.bin").write_bytes(struct.pack("<96H",*(obj_palette+button_pal+sign_pal+pickup_pal+skull_pal+paint_pal)))
(OUT/"button.bin").write_bytes(button_tiles)
(OUT/"sign.bin").write_bytes(sign_tiles)
(OUT/"dialogs.bin").write_bytes(dialog_bytes)
(OUT/"pickups.bin").write_bytes(pickup_tiles)
(OUT/"skull.bin").write_bytes(skull_tiles)
(OUT/"paint.bin").write_bytes(paint_tiles)
(OUT/"paintbase.bin").write_bytes(paint_base_tiles)
(OUT/"kitchen.bin").write_bytes(kitchen_tiles)
(OUT/"kitchenpal.bin").write_bytes(struct.pack("<16H",*kitchen_palette))
(OUT/"bossbar.bin").write_bytes(bossbar_tiles)
(OUT/"bossbarpal.bin").write_bytes(struct.pack("<16H",*bossbar_palette))
(OUT/"trashrot.bin").write_bytes(trash_rot_pixels)
(OUT/"trashrotspans.bin").write_bytes(trash_rot_spans)
(OUT/"digits.bin").write_bytes(hud_digits)
(OUT/"enemies.bin").write_bytes(enemy_pixels)
(OUT/"enemyspans.bin").write_bytes(enemy_spans)
(OUT/"menu.bin").write_bytes(menu_pixels)
(OUT/"menupal.bin").write_bytes(struct.pack("<256H",*menu_palette))
(OUT/"pause.bin").write_bytes(pause_pixels)
(OUT/"pausepal.bin").write_bytes(struct.pack("<256H",*pause_palette))
(OUT/"bosspatches.bin").write_bytes(boss_patch_pixels)
(OUT/"bosspatches_odd.bin").write_bytes(boss_patch_pixels_odd)
(OUT/"titles.bin").write_bytes(title_tiles)
(OUT/"titlepal.bin").write_bytes(struct.pack("<160H",*title_palette))

# Ending screens use their original pixels. The 240x160 LCD shows a native-size
# crop; the true ending retains its original 90-pixel horizontal pan.
end_images=[rgba("spr_end"),rgba("spr_trueend")]
end_strip=Image.new("RGB",(sum(i.width for i in end_images),max(i.height for i in end_images)),(0,0,0))
xx=0
for image in end_images:end_strip.paste(image.convert("RGB"),(xx,0));xx+=image.width
end_q=end_strip.quantize(colors=256,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
end_p=(end_q.getpalette() or [])[:768];end_p += [0]*(768-len(end_p));end_palette=[]
for n in range(256):
    rr,gg,bb=end_p[n*3:n*3+3];end_palette.append((rr>>3)|((gg>>3)<<5)|((bb>>3)<<10))
end_pixels=bytearray();xx=0
for image in end_images:
    end_pixels.extend(end_q.crop((xx,0,xx+image.width,image.height)).tobytes());xx+=image.width
(OUT/"end.bin").write_bytes(end_pixels)
(OUT/"endpal.bin").write_bytes(struct.pack("<256H",*end_palette))

# Direct Sound assets. 16384 Hz divides the GBA master clock exactly (1024
# clocks/sample), which keeps pitch stable and makes Timer0 setup inexpensive.
ffmpeg=shutil.which("ffmpeg") or str(PROJECT.parent.parent/"work/ffmpeg/ffmpeg-9.0.1-essentials_build/bin/ffmpeg.exe")
if not Path(ffmpeg).exists(): raise RuntimeError("ffmpeg não encontrado para converter áudio GBA")
def pcm(sound_name,volume):
    source=PROJECT/sounds[sound_name]["path"]
    result=subprocess.run([ffmpeg,"-v","error","-i",str(source),"-ac","1","-ar","16384","-af",f"volume={volume}","-f","s8","-"],capture_output=True,check=True)
    raw=result.stdout
    # Remove only digital near-silence before the transient, retaining 2 ms of
    # preroll. This compensates the original files' 25-82 ms empty prefixes.
    first=next((i for i,value in enumerate(raw) if abs(value if value<128 else value-256)>=2),0)
    return raw[max(0,first-33):]
sfx_names=["sfx_jump","sfx_coin","sfx_hurt","sfx_blink","sfx_boss2","sfx_bossroar"]
sfx_data=bytearray(); sfx_meta=[]
for name in sfx_names:
    raw=pcm(name,sounds[name]["volume"]); length=len(raw); raw+=bytes((-len(raw))&3)+bytes(1024)
    sfx_meta.append((len(sfx_data),length)); sfx_data.extend(raw)
(OUT/"sfx.bin").write_bytes(sfx_data)
# Keep old generated trees deterministic after upgrading from a music-enabled
# build; this file is deliberately empty and is not linked into the ROM.
(OUT/"music.bin").write_bytes(b"")

with (OUT/"assets.h").open("w",encoding="ascii") as f:
    f.write("#pragma once\n#include <stdint.h>\n")
    f.write("typedef struct { uint16_t w,h,start_x,start_y; uint32_t pixels,collision,platform,hazard,slope,palette; uint16_t sign_first,sign_count,pickup_first,pickup_count,enemy_first,enemy_count,next_x,next_y,next_w,next_h; } RoomAsset;\n")
    f.write("typedef struct { uint16_t x,y; uint8_t kind,pad; } SignAsset;\n")
    f.write("typedef struct { uint16_t x,y; uint8_t kind,pad; } PickupAsset;\n")
    f.write("typedef struct { uint32_t offset,spans; uint16_t w,h; int16_t ox,oy; uint16_t frames; } EnemySpriteAsset;\n")
    f.write("typedef struct { int32_t x,y; uint16_t sprite,spawn_sprite,alt_sprite; uint8_t behavior; int8_t state; int16_t timer,direction; uint16_t range; int16_t speed,vertical_speed; uint16_t spawn_rate; int16_t spawn_speed,alt_spawn_speed,scale_x,scale_y; uint8_t harmful,flags,anim_period,pad; } EnemyAsset;\n")
    f.write("typedef struct { uint32_t offset; uint16_t length; int8_t x_offset,y_offset; uint8_t blocks,bottom_x,bottom_count,pad; } TitleAsset;\n")
    f.write("typedef struct { uint32_t offset,length; } AudioAsset;\n")
    f.write("typedef struct { uint32_t offset; uint16_t x,y,w,h; } BossPatchAsset;\n")
    f.write("typedef struct { uint16_t sprite,tile,frame_tiles; uint8_t w,h; int8_t ox,oy; } KitchenSpriteAsset;\n")
    f.write(f"#define ROOM_COUNT {len(metadata)}\n#define PLAYER_FRAME_COUNT {len(frames)}\n#define ENEMY_MAX 64\n")
    f.write("extern const unsigned char room_pixels[], room_pixels_odd[], collision_data[], platform_data[], hazard_data[], slope_data[], palette_data[], player_tiles[], trail_tiles[], obj_palette[], button_tiles[], sign_tiles[], dialog_data[], pickup_tiles[], skull_tiles[], paint_tiles[], paint_base_tiles[], kitchen_tiles[], kitchen_palette[], bossbar_tiles[], bossbar_palette[], trash_rot_pixels[], trash_rot_spans[], hud_digits[], enemy_pixels[], enemy_spans[], menu_pixels[], menu_palette[], pause_pixels[], pause_palette[], boss_patch_pixels[], boss_patch_pixels_odd[], end_pixels[], end_palette[], title_tiles[], title_palette[], sfx_data[];\n")
    f.write("static const RoomAsset room_assets[ROOM_COUNT]={\n")
    for m in metadata: f.write("{"+",".join(map(str,m))+"},\n")
    f.write("};\nstatic const SignAsset sign_assets[]={\n")
    for s in signs: f.write(f"{{{s[0]},{s[1]},{s[2]},0}},\n")
    f.write("};\nstatic const PickupAsset pickup_assets[]={\n")
    for p in pickups: f.write(f"{{{p[0]},{p[1]},{p[2]},0}},\n")
    f.write("};\nstatic const uint8_t player_group[]={"+",".join(map(str,frame_groups))+"};\n")
    f.write("static const EnemySpriteAsset enemy_sprite_assets[]={\n")
    for s in enemy_sprites: f.write("{"+",".join(map(str,s))+"},\n")
    f.write("};\nstatic const EnemyAsset enemy_assets[]={\n")
    for e in enemies: f.write("{"+",".join(map(str,e))+"},\n")
    f.write("};\n")
    if boss_sprite_ids: f.write("static const uint16_t boss_sprite_ids[]={"+",".join(map(str,boss_sprite_ids))+"};\n")
    f.write("static const uint16_t blink_sprite_ids[]={"+",".join(map(str,blink_sprite_ids))+"};\n")
    f.write(f"#define BOSSBAR_SPRITE_ID {bossbar_sprite_id}\n")
    f.write("static const KitchenSpriteAsset kitchen_sprite_assets[]={"+",".join("{"+",".join(map(str,t))+"}" for t in kitchen_meta)+"};\n")
    f.write(f"#define KITCHEN_SPRITE_FIRST {kitchen_ids[0] if kitchen_ids else 0}\n")
    f.write(f"#define KITCHEN_SPRITE_COUNT {len(kitchen_meta)}\n")
    f.write(f"#define KITCHEN_TILE_HALFWORDS {len(kitchen_tiles)//2}\n")
    f.write("static const uint16_t bossbar_tile[]={"+",".join(str(t[0]) for t in bossbar_meta)+"};\n")
    f.write(f"#define BOSSBAR_TILE_HALFWORDS {len(bossbar_tiles)//2}\n")
    f.write(f"#define TRASH_ROT_W {trash_rot_w}\n#define TRASH_ROT_H {trash_rot_h}\n#define TRASH_ROT_OX {trash_rot_ox}\n#define TRASH_ROT_OY {trash_rot_oy}\n")
    f.write("static const uint16_t boss_patch_first[]={"+",".join(map(str,boss_patch_first))+"};\n")
    f.write("static const BossPatchAsset boss_patch_assets[]={"+",".join("{"+",".join(map(str,t))+"}" for t in boss_patch_meta)+"};\n")
    f.write(f"static const TitleAsset title_assets[{len(title_meta)}]={{"+",".join("{"+",".join(map(str,t))+",0}" for t in title_meta)+"};\n")
    starts=[]; counts=[]
    for gi in range(len(groups)):
        ids=[i for i,g in enumerate(frame_groups) if g==gi]; starts.append(ids[0]); counts.append(len(ids))
    f.write("static const uint8_t group_start[]={"+",".join(map(str,starts))+"};\n")
    f.write("static const uint8_t group_count[]={"+",".join(map(str,counts))+"};\n")
    f.write("static const AudioAsset sfx_assets[]={"+",".join("{"+str(o)+","+str(n)+"}" for o,n in sfx_meta)+"};\n")

with (OUT/"assets.s").open("w",encoding="ascii") as f:
    f.write('.section .rodata\n.align 2\n')
    for sym,file in [("room_pixels","rooms.bin"),("room_pixels_odd","rooms_odd.bin"),("collision_data","collision.bin"),("platform_data","platform.bin"),("hazard_data","hazard.bin"),("slope_data","slope.bin"),
                     ("palette_data","palettes.bin"),("player_tiles","player.bin"),("trail_tiles","trails.bin"),("obj_palette","objpal.bin"),("button_tiles","button.bin"),("sign_tiles","sign.bin"),("dialog_data","dialogs.bin"),("pickup_tiles","pickups.bin"),("skull_tiles","skull.bin"),("paint_tiles","paint.bin"),("paint_base_tiles","paintbase.bin"),("kitchen_tiles","kitchen.bin"),("kitchen_palette","kitchenpal.bin"),("bossbar_tiles","bossbar.bin"),("bossbar_palette","bossbarpal.bin"),("trash_rot_pixels","trashrot.bin"),("trash_rot_spans","trashrotspans.bin"),("hud_digits","digits.bin"),("enemy_pixels","enemies.bin"),("enemy_spans","enemyspans.bin"),("menu_pixels","menu.bin"),("menu_palette","menupal.bin"),("pause_pixels","pause.bin"),("pause_palette","pausepal.bin"),("boss_patch_pixels","bosspatches.bin"),("boss_patch_pixels_odd","bosspatches_odd.bin"),("end_pixels","end.bin"),("end_palette","endpal.bin"),("title_tiles","titles.bin"),("title_palette","titlepal.bin"),("sfx_data","sfx.bin")]:
        f.write(f'.global {sym}\n{sym}:\n.incbin "generated/{file}"\n.align 2\n')

print(f"Total de assets: {(sum(p.stat().st_size for p in OUT.glob('*.bin'))/1048576):.2f} MiB")
