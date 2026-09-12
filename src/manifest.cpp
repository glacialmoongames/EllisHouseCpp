#include "manifest.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) result.push_back(item);
    return result;
}

int integer(const std::vector<std::string>& fields, std::size_t i) { return std::stoi(fields.at(i)); }
float number(const std::vector<std::string>& fields, std::size_t i) { return std::stof(fields.at(i)); }
std::uint32_t colour(const std::vector<std::string>& fields, std::size_t i) {
    return static_cast<std::uint32_t>(std::stoull(fields.at(i)));
}
}

GameData loadManifest(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Could not open manifest: " + path.string());
    GameData data;
    RoomDef* room = nullptr;
    FontDef* font = nullptr;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto f = split(line, '\t');
        if (f.empty() || f[0].empty() || f[0] == "ELLIS_HOUSE_MANIFEST") continue;
        if (f[0] == "SPRITE") {
            SpriteDef value{f.at(1), integer(f,2), integer(f,3), integer(f,4), integer(f,5),
                integer(f,6), integer(f,7), integer(f,8), integer(f,9), integer(f,10), number(f,11), split(f.at(12), ';')};
            value.separateMask = f.size()>13 && integer(f,13)!=0;
            if (value.frames.size() == 1 && value.frames[0].empty()) value.frames.clear();
            data.sprites.emplace(value.name, std::move(value));
        } else if (f[0] == "OBJECT") {
            ObjectDef value{f.at(1), f.at(2), f.at(3), f.at(7), f.at(8), integer(f,4)!=0, integer(f,5)!=0,
                integer(f,6)!=0, number(f,9), number(f,10), number(f,11), number(f,12), number(f,13),
                number(f,14), number(f,15), number(f,16), number(f,17)};
            data.objects.emplace(value.name, std::move(value));
        } else if (f[0] == "ROOM") {
            RoomDef value{f.at(1), integer(f,2), integer(f,3), integer(f,4), integer(f,5),
                integer(f,6)!=0, number(f,7), number(f,8), number(f,9), number(f,10), {}, {}, {}};
            auto [it, inserted] = data.rooms.emplace(value.name, std::move(value));
            (void)inserted;
            room = &it->second;
        } else if (f[0] == "BACKGROUND" && room) {
            room->backgrounds.push_back({integer(f,1), f.at(2), colour(f,3), integer(f,4)!=0,
                integer(f,5)!=0, integer(f,6)!=0, number(f,7), number(f,8),
                f.size()>9 ? number(f,9) : 0.0F, f.size()>10 ? number(f,10) : 0.0F});
        } else if (f[0] == "GRAPHIC" && room) {
            room->graphics.push_back({integer(f,1), f.at(2), number(f,3), number(f,4), number(f,5),
                number(f,6), number(f,7), number(f,8), number(f,9), number(f,10), number(f,11), colour(f,12)});
        } else if (f[0] == "INSTANCE" && room) {
            room->instances.push_back({integer(f,1), f.at(2), f.at(3), "", number(f,4), number(f,5),
                number(f,6), number(f,7), number(f,8), colour(f,9), number(f,10), number(f,11), true,
                0, 0, 0, 0, 0, 0, 0, 0});
        } else if (f[0] == "ENDROOM") {
            room = nullptr;
        } else if (f[0] == "ROOMORDER") {
            data.roomOrder = split(f.at(1), ';');
        } else if (f[0] == "SOUND") {
            SoundDef value{f.at(1), f.at(2), number(f,3), number(f,4)};
            data.sounds.emplace(value.name, std::move(value));
        } else if (f[0] == "FONT") {
            data.font = {f.at(1), f.at(2), number(f,3), {}};
            font = &data.font;
        } else if (f[0] == "GLYPH" && font) {
            font->glyphs.push_back({integer(f,1),integer(f,2),integer(f,3),integer(f,4),integer(f,5),integer(f,6),integer(f,7)});
        } else if (f[0] == "ENDFONT") {
            font = nullptr;
        }
    }
    return data;
}
