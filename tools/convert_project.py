#!/usr/bin/env python3
"""Convert the data portion of a GameMaker 2024 project to the Ellis House C++ manifest.

This deliberately uses only the Python standard library so the conversion remains
repeatable on a clean checkout. GameMaker's .yy files are JSON with trailing commas.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
from pathlib import Path


def read_yy(path: Path) -> dict:
    text = path.read_text(encoding="utf-8-sig")
    text = re.sub(r",(?=\s*[}\]])", "", text)
    return json.loads(text)


def clean(value: object) -> str:
    return str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ")


def emit(out, *values: object) -> None:
    out.write("\t".join(clean(v) for v in values) + "\n")


def copy_sprite_frames(source: Path, destination: Path, sprite: dict) -> list[str]:
    name = sprite["name"]
    frame_paths: list[str] = []
    target = destination / "sprites" / name
    target.mkdir(parents=True, exist_ok=True)
    for frame in sprite.get("frames", []):
        frame_name = frame["name"]
        src = source / "sprites" / name / f"{frame_name}.png"
        if not src.exists():
            continue
        dst = target / f"{frame_name}.png"
        shutil.copy2(src, dst)
        frame_paths.append(f"assets/sprites/{name}/{frame_name}.png")
    return frame_paths


def sound_extension(data: bytes) -> str:
    if data.startswith(b"RIFF"):
        return ".wav"
    if data.startswith(b"ID3") or data[:2] in (b"\xff\xfb", b"\xff\xf3", b"\xff\xf2"):
        return ".mp3"
    if len(data) > 12 and data[4:12] in (b"ftypM4A ", b"ftypisom", b"ftypmp42"):
        return ".m4a"
    return ".bin"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    assets = output / "assets"
    assets.mkdir(parents=True, exist_ok=True)

    sprites: dict[str, dict] = {}
    for yy_path in sorted((source / "sprites").glob("*/*.yy")):
        sprite = read_yy(yy_path)
        sprites[sprite["name"]] = sprite

    objects: dict[str, dict] = {}
    for yy_path in sorted((source / "objects").glob("*/*.yy")):
        obj = read_yy(yy_path)
        step_path = yy_path.parent / "Step_0.gml"
        create_path = yy_path.parent / "Create_0.gml"
        step = step_path.read_text(encoding="utf-8-sig") if step_path.exists() else ""
        create = create_path.read_text(encoding="utf-8-sig") if create_path.exists() else ""
        behavior = "none"
        for marker, label in (("scr_eslide_boss", "eslide_boss"), ("scr_ethrow_range", "ethrow_range"),
                              ("scr_efollow", "efollow"), ("scr_espawner", "espawner"),
                              ("scr_eslide", "eslide"), ("scr_efall", "efall"), ("scr_ethrow", "ethrow")):
            if marker in step:
                behavior = label
                break
        special = {"obj_trash_ball":"trash_ball", "obj_bosscar":"boss_car", "obj_eyel":"eye_left", "obj_eyer":"eye_right"}
        behavior = special.get(obj["name"], behavior)
        spawn_match = re.search(r"(?m)^\s*obj\s*=\s*(obj_[A-Za-z0-9_]+)", create)
        obj["_spawn"] = spawn_match.group(1) if spawn_match else ""
        values = {}
        for key in ("state", "timer", "type", "range", "spd", "vsp", "dir", "spawnrate", "image_speed"):
            match = re.search(rf"(?m)^\s*{key}\s*=\s*(-?\d+(?:\.\d+)?)", create)
            values[key] = float(match.group(1)) if match else 0.0
        obj["_behavior"] = behavior
        obj["_values"] = values
        obj["_has_image_speed"] = bool(re.search(r"(?m)^\s*image_speed\s*=\s*-?\d", create))
        objects[obj["name"]] = obj

    tilesets: dict[str, dict] = {}
    for yy_path in sorted((source / "tilesets").glob("*/*.yy")):
        tile = read_yy(yy_path)
        tilesets[tile["name"]] = tile

    manifest_path = assets / "game.manifest"
    with manifest_path.open("w", encoding="utf-8", newline="\n") as out:
        emit(out, "ELLIS_HOUSE_MANIFEST", 1)
        for name, sprite in sprites.items():
            frames = copy_sprite_frames(source, assets, sprite)
            sequence = sprite.get("sequence") or {}
            emit(
                out,
                "SPRITE", name, sprite.get("width", 0), sprite.get("height", 0),
                sequence.get("xorigin", 0), sequence.get("yorigin", 0),
                sprite.get("bbox_left", 0), sprite.get("bbox_top", 0),
                sprite.get("bbox_right", sprite.get("width", 1) - 1),
                sprite.get("bbox_bottom", sprite.get("height", 1) - 1),
                sprite.get("collisionKind", 1), sequence.get("playbackSpeed", 15.0), ";".join(frames),
                int(sprite.get("separateMask", False)),
            )

        for name, obj in objects.items():
            sprite = (obj.get("spriteId") or {}).get("name", "")
            parent = (obj.get("parentObjectId") or {}).get("name", "")
            v = obj["_values"]
            emit(out, "OBJECT", name, sprite, parent, int(obj.get("visible", True)),
                 int(obj.get("solid", False)), int(obj.get("persistent", False)), obj["_behavior"],
                 obj["_spawn"], v["state"], v["timer"], v["type"], v["range"], v["spd"], v["vsp"], v["dir"],
                 v["spawnrate"], v["image_speed"] if obj["_has_image_speed"] else -1)

        for name, tile in tilesets.items():
            emit(out, "TILESET", name, (tile.get("spriteId") or {}).get("name", ""),
                 tile.get("tileWidth", 16), tile.get("tileHeight", 16),
                 tile.get("tilexoff", 0), tile.get("tileyoff", 0),
                 tile.get("tilehsep", 0), tile.get("tilevsep", 0),
                 tile.get("out_columns", 1))

        for font_path in sorted((source / "fonts").glob("*/*.yy")):
            font = read_yy(font_path)
            image = font_path.with_suffix(".png")
            target = assets / "fonts" / image.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(image, target)
            emit(out, "FONT", font["name"], f"assets/fonts/{image.name}", font.get("size", 12))
            for glyph in font.get("glyphs", {}).values():
                emit(out, "GLYPH", glyph["character"], glyph["x"], glyph["y"], glyph["w"], glyph["h"],
                     glyph.get("offset", 0), glyph.get("shift", glyph["w"]))
            emit(out, "ENDFONT")

        for yy_path in sorted((source / "rooms").glob("*/*.yy")):
            room = read_yy(yy_path)
            settings = room.get("roomSettings", {})
            views = room.get("views") or [{}]
            view = next((v for v in views if v.get("visible")), views[0])
            placed_players = [i for layer in room.get("layers", [])
                              for i in layer.get("instances", [])
                              if (i.get("objectId") or {}).get("name") == "obj_player"]
            creation_path = yy_path.parent / room.get("creationCodeFile", "")
            creation = creation_path.read_text(encoding="utf-8-sig") if creation_path.is_file() else ""
            create_match = re.search(r"instance_create\s*\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*obj_player", creation)
            spawn_x = placed_players[0].get("x", 0) if placed_players else (float(create_match.group(1)) if create_match else 0)
            spawn_y = placed_players[0].get("y", 0) if placed_players else (float(create_match.group(2)) if create_match else 0)
            respawn_x = re.search(r"respawnx\s*=\s*([-\d.]+)", creation)
            respawn_y = re.search(r"respawny\s*=\s*([-\d.]+)", creation)
            emit(out, "ROOM", room["name"], settings.get("Width", 384), settings.get("Height", 218),
                 view.get("wview", 384), view.get("hview", 218), int(bool(placed_players or create_match)),
                 spawn_x, spawn_y, respawn_x.group(1) if respawn_x else spawn_x,
                 respawn_y.group(1) if respawn_y else spawn_y)
            for layer in room.get("layers", []):
                if not layer.get("visible", True):
                    continue
                depth = layer.get("depth", 0)
                kind = layer.get("resourceType", "")
                if kind == "GMRBackgroundLayer":
                    emit(out, "BACKGROUND", depth, (layer.get("spriteId") or {}).get("name", ""),
                         layer.get("colour", 0xFFFFFFFF), int(layer.get("stretch", False)),
                         int(layer.get("htiled", False)), int(layer.get("vtiled", False)),
                         layer.get("x", 0), layer.get("y", 0),
                         layer.get("hspeed", 0), layer.get("vspeed", 0))
                elif kind == "GMRAssetLayer":
                    for graphic in layer.get("assets", []):
                        if graphic.get("ignore", False):
                            continue
                        emit(out, "GRAPHIC", depth, (graphic.get("spriteId") or {}).get("name", ""),
                             graphic.get("x", 0), graphic.get("y", 0), graphic.get("u0", 0),
                             graphic.get("v0", 0), graphic.get("w", 0), graphic.get("h", 0),
                             graphic.get("scaleX", 1), graphic.get("scaleY", 1),
                             graphic.get("rotation", 0), graphic.get("colour", 0xFFFFFFFF))
                elif kind == "GMRInstanceLayer":
                    for instance in layer.get("instances", []):
                        if instance.get("ignore", False):
                            continue
                        emit(out, "INSTANCE", depth, instance.get("name", ""),
                             (instance.get("objectId") or {}).get("name", ""),
                             instance.get("x", 0), instance.get("y", 0),
                             instance.get("scaleX", 1), instance.get("scaleY", 1),
                             instance.get("rotation", 0), instance.get("colour", 0xFFFFFFFF),
                             instance.get("imageIndex", 0), instance.get("imageSpeed", 1))
            emit(out, "ENDROOM")

        # Room order comes from the project file and is independent of alphabetical paths.
        yyp_path = next(source.glob("*.yyp"))
        project = read_yy(yyp_path)
        emit(out, "ROOMORDER", ";".join(node["roomId"]["name"] for node in project.get("RoomOrderNodes", [])
                                         if node["roomId"]["name"] != "rm_5"))

        sound_target = assets / "sounds"
        sound_target.mkdir(exist_ok=True)
        for yy_path in sorted((source / "sounds").glob("*/*.yy")):
            sound = read_yy(yy_path)
            raw = yy_path.parent / sound.get("soundFile", sound["name"])
            if not raw.exists():
                continue
            data = raw.read_bytes()
            extension = sound_extension(data[:32])
            target_name = sound["name"] + extension
            ffmpeg = os.environ.get("FFMPEG") or shutil.which("ffmpeg")
            existing_ogg = sound_target / (sound["name"] + ".ogg")
            if extension == ".m4a" and ffmpeg:
                target_name = sound["name"] + ".ogg"
                subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                                "-i", str(raw), "-vn", "-c:a", "libvorbis", "-q:a", "6",
                                str(sound_target / target_name)], check=True)
            elif extension == ".m4a" and existing_ogg.exists():
                target_name = existing_ogg.name
            else:
                shutil.copy2(raw, sound_target / target_name)
            for obsolete in sound_target.glob(sound["name"] + ".*"):
                if obsolete.name != target_name:
                    obsolete.unlink()
            emit(out, "SOUND", sound["name"], f"assets/sounds/{target_name}",
                 sound.get("volume", 1.0), sound.get("duration", 0.0))

    # Preserve every script/event for parity work and traceability.
    reference = output / "reference" / "gml"
    if reference.exists():
        shutil.rmtree(reference)
    for gml in source.rglob("*.gml"):
        relative = gml.relative_to(source)
        target = reference / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(gml, target)

    print(f"Converted {len(sprites)} sprites, {len(objects)} objects and "
          f"{len(list((source / 'rooms').glob('*/*.yy')))} rooms")
    print(manifest_path)


if __name__ == "__main__":
    main()
