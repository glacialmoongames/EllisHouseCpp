#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SpriteDef {
    std::string name;
    int width{}, height{}, originX{}, originY{};
    int bboxLeft{}, bboxTop{}, bboxRight{}, bboxBottom{};
    int collisionKind{1};
    float fps{15.0F};
    std::vector<std::string> frames;
    bool separateMask{};
};

struct ObjectDef {
    std::string name, sprite, parent, behavior, spawnObject;
    bool visible{true}, solid{false}, persistent{false};
    float state{}, timer{}, type{}, range{}, speed{}, verticalSpeed{}, direction{}, spawnRate{}, animationSpeed{-1};
};

struct BackgroundDef {
    int depth{};
    std::string sprite;
    std::uint32_t colour{0xFFFFFFFF};
    bool stretch{}, tileX{}, tileY{};
    float x{}, y{}, hspeed{}, vspeed{};
};

struct GraphicDef {
    int depth{};
    std::string sprite;
    float x{}, y{}, u0{}, v0{}, width{}, height{}, scaleX{1}, scaleY{1}, rotation{};
    std::uint32_t colour{0xFFFFFFFF};
};

struct InstanceDef {
    int depth{};
    std::string name, object, spriteOverride;
    float x{}, y{}, scaleX{1}, scaleY{1}, rotation{};
    std::uint32_t colour{0xFFFFFFFF};
    float imageIndex{}, imageSpeed{1};
    bool active{true};
    float state{}, timer{}, type{}, range{}, speed{}, verticalSpeed{}, direction{}, imageAngle{};
    float previousX{}, previousY{};
};

struct RoomDef {
    std::string name;
    int width{}, height{}, viewWidth{}, viewHeight{};
    bool hasPlayer{};
    float spawnX{}, spawnY{}, respawnX{}, respawnY{};
    std::vector<BackgroundDef> backgrounds;
    std::vector<GraphicDef> graphics;
    std::vector<InstanceDef> instances;
};

struct SoundDef {
    std::string name, path;
    float volume{1}, duration{};
};

struct GlyphDef { int value{}, x{}, y{}, width{}, height{}, offset{}, advance{}; };
struct FontDef { std::string name, path; float size{12}; std::vector<GlyphDef> glyphs; };

struct GameData {
    std::unordered_map<std::string, SpriteDef> sprites;
    std::unordered_map<std::string, ObjectDef> objects;
    std::unordered_map<std::string, RoomDef> rooms;
    std::unordered_map<std::string, SoundDef> sounds;
    FontDef font;
    std::vector<std::string> roomOrder;
};
