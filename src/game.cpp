#include "game.hpp"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#if defined(__3DS__) || defined(__PSP__)
#include <sys/stat.h>
#endif

namespace {
constexpr int kViewWidth = 384;
constexpr int kViewHeight = 218;
constexpr float kGravity = 0.6F;

float sign(float value) { return (value > 0) - (value < 0); }
float mix(float previous,float current,float alpha) { return previous+(current-previous)*alpha; }
void beginGmSubtract() {
    // GameMaker's bm_subtract is not the OpenGL reverse-subtract equation.
    // It is (bm_zero, bm_inv_src_colour), i.e. dst * (1 - src).
    rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE_MINUS_SRC_COLOR, RL_ZERO, RL_ONE,
                              RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
}
#ifndef __PSP__
const std::unordered_map<std::string,std::string>& roomMusicMapping() {
    static const std::unordered_map<std::string,std::string> mapping{
        {"rm_menu","snd_menu"},{"rm_0","snd_01"},{"rm_1","snd_02"},{"rm_2","snd_03"},
        {"rm_3","snd_04"},{"rm_4","snd_05"},{"rm_5","snd_06"},{"rm_boss","snd_boss"},{"rm_end","snd_end"}};
    return mapping;
}
#endif
}

Game::Game(GameData data, std::filesystem::path root) : data_(std::move(data)), root_(std::move(root)) {
#ifdef __EMSCRIPTEN__
    // A resizable raylib web window adopts the browser viewport as its
    // framebuffer. On portrait phones that creates a tall canvas which CSS
    // then squeezes into the landscape game area. Keep the framebuffer at the
    // game's native 2x resolution and only scale its CSS presentation.
    SetConfigFlags(FLAG_VSYNC_HINT);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#endif
    InitWindow(kViewWidth * 2, kViewHeight * 2, "Elli's House");
#ifdef __EMSCRIPTEN__
    // Some mobile browsers still initialize GLFW from the portrait viewport.
    // Programmatically restore both raylib's screen/FBO state and the HTML
    // canvas backing store after GLFW has installed its browser callbacks.
    ClearWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowSize(kViewWidth * 2, kViewHeight * 2);
#endif
    SetExitKey(KEY_F10);
#ifdef __EMSCRIPTEN__
    // raylib's default stream halves hold only 1/30 s of audio. Mobile
    // browsers regularly miss that deadline while decoding/rendering, which
    // makes the mixer output gaps perceived as severe distortion.
    SetAudioStreamBufferSizeDefault(16384);
#endif
    InitAudioDevice();
    loadFont();
    // Small effects stay resident so the first jump/pickup never performs I/O.
    for (const auto& [name, definition] : data_.sounds) {
        if (!name.starts_with("sfx_")) continue;
        Sound loaded=LoadSound((root_/definition.path).string().c_str());
        if (IsSoundValid(loaded)) sounds_.emplace(name,loaded);
    }
    // VSync drives presentation; gameplay remains a deterministic 45 Hz fixed step.
    SetTargetFPS(120);
    target_ = LoadRenderTexture(kViewWidth, kViewHeight);
    lightTarget_ = LoadRenderTexture(kViewWidth, kViewHeight);
    SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
    camera_.offset = {kViewWidth / 2.0F, kViewHeight / 2.0F};
    camera_.zoom = 1.0F;
    loadSave();
    if (data_.roomOrder.empty()) throw std::runtime_error("manifest has no rooms");
    for (const auto& [name, room] : data_.rooms) roomTemplates_[name] = room.instances;
    roomIndex_ = 0;
    loadRoom(roomIndex_);
    if (const char* requested = std::getenv("ELLIS_START_ROOM")) {
        auto found = std::find(data_.roomOrder.begin(), data_.roomOrder.end(), requested);
        if (found != data_.roomOrder.end()) loadRoom(static_cast<std::size_t>(found-data_.roomOrder.begin()));
    }
    if (const char* x = std::getenv("ELLIS_PLAYER_X")) player_.x=std::strtof(x,nullptr);
    if (const char* y = std::getenv("ELLIS_PLAYER_Y")) player_.y=std::strtof(y,nullptr);
    player_.previousX=player_.x; player_.previousY=player_.y;
    if (const char* move = std::getenv("ELLIS_QA_MOVE")) qaHorizontal_=std::clamp(std::strtof(move,nullptr),-1.0F,1.0F);
    if (std::getenv("ELLIS_QA_SLIDE")) { qaDown_=true; haveSlide_=true; }
    if (const char* jump = std::getenv("ELLIS_QA_JUMP_PERIOD")) qaJumpPeriod_=std::max(0,std::atoi(jump));
    if (const char* screenshot = std::getenv("ELLIS_SCREENSHOT")) screenshotPath_ = screenshot;
    if (const char* frame = std::getenv("ELLIS_SCREENSHOT_FRAME")) screenshotFrame_ = std::max(1, std::atoi(frame));
    if (!screenshotPath_.empty()) { ClearWindowState(FLAG_VSYNC_HINT); SetTargetFPS(0); }
}

void Game::loadSave() {
#ifdef __3DS__
    std::ifstream input("sdmc:/3ds/EllisHouse3DS/Save.sav");
#elif defined(__PSP__)
    const std::string device=root_.generic_string().starts_with("ef0:") ? "ef0:" : "ms0:";
    std::ifstream input(device+"/PSP/SAVEDATA/ELLISHOUSE/Save.sav");
#else
    std::ifstream input(root_ / "Save.sav");
#endif
    std::string line;
    while (std::getline(input, line)) {
        if (line.starts_with("Game+=")) gameCompleted_ = std::stoi(line.substr(6)) != 0;
        else if (line.starts_with("Frames=")) bestFrames_ = std::stoi(line.substr(7));
        else if (line.starts_with("Milisecond=")) bestFrames_ = static_cast<int>(std::stof(line.substr(11)));
    }
}

void Game::saveGame() const {
#ifdef __3DS__
    mkdir("sdmc:/3ds/EllisHouse3DS",0777);
    std::ofstream output("sdmc:/3ds/EllisHouse3DS/Save.sav", std::ios::trunc);
#elif defined(__PSP__)
    const std::string device=root_.generic_string().starts_with("ef0:") ? "ef0:" : "ms0:";
    mkdir((device+"/PSP/SAVEDATA").c_str(),0777);
    mkdir((device+"/PSP/SAVEDATA/ELLISHOUSE").c_str(),0777);
    std::ofstream output(device+"/PSP/SAVEDATA/ELLISHOUSE/Save.sav",std::ios::trunc);
#else
    std::ofstream output(root_ / "Save.sav", std::ios::trunc);
#endif
    output << "[Save]\nGame+=" << (gameCompleted_ ? 1 : 0)
           << "\nTime=" << bestFrames_*3 << "\nSecond=" << bestFrames_
           << "\nMinute=" << bestFrames_ << "\nMilisecond=" << bestFrames_ << '\n';
}

std::string Game::playerSprite(const std::string& normal) const {
    if (!plusMode_ || skin_ < 0 || !normal.starts_with("spr_player_")) return normal;
    return std::string(skin_ == 0 ? "spr_mib_" : "spr_pope_") + normal.substr(11);
}

Game::~Game() {
    for (auto& [name, frames] : textures_) {
        (void)name;
        for (auto& texture : frames.frames) UnloadTexture(texture);
    }
    for (auto& [name, sound] : sounds_) { (void)name; UnloadSound(sound); }
#ifndef __PSP__
    if (musicLoaded_) UnloadMusicStream(music_);
#endif
    if (fontLoaded_) UnloadFont(font_);
    UnloadRenderTexture(target_);
    UnloadRenderTexture(lightTarget_);
    CloseAudioDevice();
    CloseWindow();
}

void Game::loadFont() {
    if (data_.font.path.empty() || data_.font.glyphs.empty()) return;
    font_.texture = LoadTexture((root_ / data_.font.path).string().c_str());
    font_.baseSize = 16;
    font_.glyphCount = static_cast<int>(data_.font.glyphs.size());
    font_.glyphPadding = 0;
    font_.recs = static_cast<Rectangle*>(MemAlloc(sizeof(Rectangle) * font_.glyphCount));
    font_.glyphs = static_cast<GlyphInfo*>(MemAlloc(sizeof(GlyphInfo) * font_.glyphCount));
    for (int i = 0; i < font_.glyphCount; ++i) {
        const auto& g = data_.font.glyphs[static_cast<std::size_t>(i)];
        font_.recs[i] = {(float)g.x,(float)g.y,(float)g.width,(float)g.height};
        font_.glyphs[i] = {g.value,g.offset,0,g.advance,{}};
    }
    fontLoaded_ = true;
}

void Game::text(const char* value, float x, float y, float size, Color color) const {
    if (fontLoaded_) DrawTextEx(font_, value, {x,y}, size, 0, color);
    else DrawText(value, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
}

float Game::textWidth(const char* value, float size) const {
    return fontLoaded_ ? MeasureTextEx(font_, value, size, 0).x : (float)MeasureText(value, static_cast<int>(size));
}

void Game::run() {
    previousFrameTime_=GetTime();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* context) {
        static_cast<Game*>(context)->runFrame();
    },this,0,true);
#else
    // runFrame() is the single input pump. Console backends update their
    // pressed-edge state inside WindowShouldClose(), so polling here as well
    // consumes every button press before the fixed-step input code sees it.
    while (running_) runFrame();
#endif
}

void Game::runFrame() {
    constexpr double fixedStep=1.0/45.0;
    if(WindowShouldClose())running_=false;
#ifdef __EMSCRIPTEN__
    // GLFW can deliver one delayed portrait resize after InitWindow(). Repair
    // it before input mapping or drawing so the render target is never scaled
    // through a portrait framebuffer, even for a single frame.
    if (GetScreenWidth()!=kViewWidth*2 || GetScreenHeight()!=kViewHeight*2) {
        ClearWindowState(FLAG_WINDOW_RESIZABLE);
        SetWindowSize(kViewWidth*2,kViewHeight*2);
    }
#endif
    if(!running_) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }
    pollInput();
#ifdef __EMSCRIPTEN__
    // Refill once per browser presentation frame, never several times during
    // a fixed-step catch-up burst.
    if (musicLoaded_) UpdateMusicStream(music_);
#endif
    if (!screenshotPath_.empty()) {
        snapshotForInterpolation();
        update();
        clearInputPulse();
        renderAlpha_=1.0F;
    } else {
        const double now=GetTime();
        frameAccumulator_+=std::min(now-previousFrameTime_,0.1);
        previousFrameTime_=now;
        int catchUp=0;
        while (frameAccumulator_>=fixedStep && catchUp++<5) {
            snapshotForInterpolation();
            update();
            clearInputPulse();
            frameAccumulator_-=fixedStep;
        }
        if (catchUp>5) frameAccumulator_=0;
        renderAlpha_=static_cast<float>(std::clamp(frameAccumulator_/fixedStep,0.0,1.0));
    }
    draw();
    if (!screenshotPath_.empty() && ++renderedFrames_ == screenshotFrame_) {
        TraceLog(LOG_INFO, "QA room=%s player=%d dead=%d paused=%d pos=%.1f,%.1f hsp=%.2f sprite=%s",
                 room_->name.c_str(), hasPlayer_, player_.dead, paused_, player_.x, player_.y,
                 player_.hsp, player_.sprite.c_str());
        TakeScreenshot(screenshotPath_.c_str());
        running_ = false;
    }
}

void Game::pollInput() {
    input_.up |= IsKeyPressed(KEY_UP)
#ifndef __3DS__
                 || IsKeyPressed(KEY_W)
#endif
                 ||
                 IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP);
    input_.down |= IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) ||
                   IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    input_.jump |= IsKeyPressed(KEY_SPACE) ||
#ifdef __3DS__
                   IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                   IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
#else
                   IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
#endif
    input_.dash |= IsKeyPressed(KEY_W)
#if !defined(__3DS__) && !defined(__PSP__)
                   || IsKeyPressed(KEY_UP)
                   || IsGamepadButtonPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP)
                   || IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT)
                   || IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_UP)
                   || IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
#endif
                   ;
    input_.confirm |= IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT) ||
                      IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
                      IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                      IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
                      IsGamepadButtonPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_UP) ||
                      IsGamepadButtonPressed(0,GAMEPAD_BUTTON_MIDDLE_RIGHT);
    input_.escape |= IsKeyPressed(KEY_ESCAPE) ||
                     IsGamepadButtonPressed(0,GAMEPAD_BUTTON_MIDDLE_RIGHT);
#ifndef __3DS__
    input_.restart |= IsKeyPressed(KEY_R);
#endif
    input_.collision |= IsKeyPressed(KEY_F1);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input_.click=true;
        const Vector2 raw=GetMousePosition();
#ifdef __3DS__
        // Touch coordinates already belong to the 320x240 lower display.
        input_.mouse=raw;
#else
        const float scale=std::min(GetScreenWidth()/(float)kViewWidth,GetScreenHeight()/(float)kViewHeight);
        input_.mouse={(raw.x-(GetScreenWidth()-kViewWidth*scale)/2)/scale,
                      (raw.y-(GetScreenHeight()-kViewHeight*scale)/2)/scale};
#endif
    }
}

void Game::snapshotForInterpolation() {
    player_.previousX=player_.x; player_.previousY=player_.y;
    previousCameraTarget_=camera_.target;
    for (auto& instance:room_->instances) {
        instance.previousX=instance.x;
        instance.previousY=instance.y;
    }
}

void Game::clearInputPulse() {
    input_=InputPulse{};
}

const SpriteDef* Game::sprite(const std::string& name) const {
    const auto it = data_.sprites.find(name);
    return it == data_.sprites.end() ? nullptr : &it->second;
}

const ObjectDef* Game::object(const std::string& name) const {
    const auto it = data_.objects.find(name);
    return it == data_.objects.end() ? nullptr : &it->second;
}

Game::TextureFrames& Game::texture(const std::string& name) {
    if (auto it = textures_.find(name); it != textures_.end()) return it->second;
    TextureFrames result;
    if (const auto* definition = sprite(name)) {
        for (const auto& relative : definition->frames) {
            auto loaded = LoadTexture((root_ / relative).string().c_str());
            SetTextureFilter(loaded, TEXTURE_FILTER_POINT);
            result.frames.push_back(loaded);
        }
    }
    auto [it, inserted] = textures_.emplace(name, std::move(result));
    (void)inserted;
    return it->second;
}

void Game::playSound(const std::string& name, float volume) {
    const auto definition = data_.sounds.find(name);
    if (definition == data_.sounds.end()) return;
    auto found = sounds_.find(name);
    if (found == sounds_.end()) {
        Sound loaded = LoadSound((root_ / definition->second.path).string().c_str());
        if (!IsSoundValid(loaded)) {
            TraceLog(LOG_WARNING, "Could not load sound effect: %s", definition->second.path.c_str());
            return;
        }
        found = sounds_.emplace(name, loaded).first;
    }
    SetSoundVolume(found->second, definition->second.volume * volume);
    PlaySound(found->second);
}

void Game::setRoomMusic() {
#ifdef __PSP__
    // PSP package intentionally contains sound effects only. Keeping this a
    // compile-time no-op removes stream I/O, mixing and room-change stalls.
    return;
#else
    const auto& mapping=roomMusicMapping();
    const auto selected = mapping.find(room_->name);
    const std::string wanted = selected == mapping.end() ? "" : selected->second;
    if (wanted == currentMusic_ && musicLoaded_) return;
    if (wanted.empty()) {
        if (musicLoaded_) { StopMusicStream(music_); UnloadMusicStream(music_); musicLoaded_ = false; }
        currentMusic_.clear();
        return;
    }
    const auto definition = data_.sounds.find(wanted);
    if (definition == data_.sounds.end()) return;
    Music nextMusic = LoadMusicStream((root_ / definition->second.path).string().c_str());
    if (!IsMusicValid(nextMusic)) {
#ifndef __3DS__
        TraceLog(LOG_WARNING, "Could not load music: %s", definition->second.path.c_str());
#endif
        return;
    }
    if (musicLoaded_) { StopMusicStream(music_); UnloadMusicStream(music_); }
    music_=nextMusic;
    currentMusic_ = wanted;
    music_.looping = true;
    const float gain=(wanted == "snd_menu") ? 1.0F : 0.5F;
    SetMusicVolume(music_, definition->second.volume*gain);
    PlayMusicStream(music_);
    musicLoaded_ = true;
#endif
}

void Game::preloadNextRoomMusic() {
#ifdef __3DS__
    const auto& mapping=roomMusicMapping();
    std::string wanted;
    for(std::size_t i=roomIndex_+1;i<data_.roomOrder.size();++i) {
        if(const auto found=mapping.find(data_.roomOrder[i]);found!=mapping.end()) {
            wanted=found->second;
            break;
        }
    }
    if(wanted.empty() && (room_->name=="rm_end" || room_->name=="rm_credits"))wanted="snd_menu";
    if(const auto definition=data_.sounds.find(wanted);definition!=data_.sounds.end())
        PreloadMusicStream((root_/definition->second.path).string().c_str());
#endif
}

bool Game::inherits(const std::string& objectName, const std::string& ancestor) const {
    // Follow references into the manifest instead of copying a std::string at
    // every parent level. Collision and rendering call this path thousands of
    // times per second on the OLD 3DS.
    const std::string* current=&objectName;
    for (int guard = 0; guard < 64 && current && !current->empty(); ++guard) {
        if (*current == ancestor) return true;
        const auto* definition = object(*current);
        current = definition ? &definition->parent : nullptr;
    }
    return false;
}

Rectangle Game::instanceBounds(const InstanceDef& instance) const {
    // These two objects account for almost every kitchen collision. Their
    // original masks are plain rectangles, so avoid manifest hash lookups on
    // every one-pixel movement probe.
    if(instance.object=="obj_colision" || instance.object=="obj_platform") {
        const float width=16.0F*instance.scaleX;
        const float height=(instance.object=="obj_platform"?1.0F:16.0F)*instance.scaleY;
        return {std::min(instance.x,instance.x+width),std::min(instance.y,instance.y+height),
                std::abs(width),std::abs(height)};
    }
    const auto* obj = object(instance.object);
    const auto* spr = obj ? sprite(instance.spriteOverride.empty() ? obj->sprite : instance.spriteOverride) : nullptr;
    if (!spr) return {instance.x, instance.y, 0, 0};
    const float left = instance.x + (spr->bboxLeft - spr->originX) * instance.scaleX;
    const float top = instance.y + (spr->bboxTop - spr->originY) * instance.scaleY;
    return {std::min(left, left + (spr->bboxRight - spr->bboxLeft + 1) * instance.scaleX),
            std::min(top, top + (spr->bboxBottom - spr->bboxTop + 1) * instance.scaleY),
            std::abs((spr->bboxRight - spr->bboxLeft + 1) * instance.scaleX),
            std::abs((spr->bboxBottom - spr->bboxTop + 1) * instance.scaleY)};
}

Rectangle Game::playerBounds(float x, float y) const {
    return playerBoundsWithProfile(x,y,playerUsesLowProfile());
}

bool Game::playerUsesLowProfile() const {
    // GameMaker selects the collision mask from the current player sprite. Keep
    // the reduced mask for the complete action, even on the transition tick in
    // which the explicit posture flag is being updated.
    return player_.lowProfile || player_.sliding ||
           player_.sprite.ends_with("_slide") ||
           player_.sprite.ends_with("_crouch") ||
           player_.sprite.ends_with("_blink");
}

Rectangle Game::playerBoundsWithProfile(float x, float y, bool lowProfile) const {
    // Exact shared masks from the original player sprites. Crouch, blink and
    // slide use bbox 3..17,10..25 around origin 10,13.
    // GameMaker evaluates sprite masks on its integer collision grid. Snapping
    // the anchor prevents a fractional remainder from making the low mask
    // overlap the refrigerator/platform edge by a fraction of one pixel.
    const float collisionX=std::round(x),collisionY=std::round(y);
    return lowProfile ? Rectangle{collisionX-7,collisionY-3,15,16}
                      : Rectangle{collisionX-7,collisionY-12,15,25};
}

const Game::CollisionMask& Game::mask(const std::string& spriteName, float frame) {
    CollisionMask result;
    const auto* definition = sprite(spriteName);
    const std::size_t frameIndex=definition && !definition->frames.empty() && definition->separateMask
        ? static_cast<std::size_t>(std::max(0.0F,std::floor(frame)))%definition->frames.size() : 0;
    const std::string key=spriteName+(definition && definition->separateMask ? "#"+std::to_string(frameIndex) : "#shared");
    if (auto it = masks_.find(key); it != masks_.end()) return it->second;
    if (definition && !definition->frames.empty()) {
        const std::size_t first=definition->separateMask ? frameIndex : 0;
        const std::size_t end=definition->separateMask ? frameIndex+1 : definition->frames.size();
        for (std::size_t f=first;f<end;++f) {
            Image image = LoadImage((root_ / definition->frames[f]).string().c_str());
            if (result.solid.empty()) {
                result.width=image.width; result.height=image.height;
                result.solid.resize(static_cast<std::size_t>(image.width*image.height));
            }
            auto* pixels=LoadImageColors(image);
            const int count=std::min(result.width*result.height,image.width*image.height);
            for (int i=0;i<count;++i) result.solid[i]=result.solid[i] || pixels[i].a!=0;
            UnloadImageColors(pixels); UnloadImage(image);
        }
    }
#ifdef __PSP__
    if(result.width>0&&result.height>0&&!result.solid.empty()) {
        result.wordsPerRow=(result.width+31)/32;
        result.rowBits.assign(static_cast<std::size_t>(result.wordsPerRow*result.height),0);
        for(int y=0;y<result.height;++y)for(int x=0;x<result.width;++x)
            if(result.solid[static_cast<std::size_t>(y*result.width+x)])
                result.rowBits[static_cast<std::size_t>(y*result.wordsPerRow+x/32)]|=1U<<(x&31);
    }
#endif
    auto [it, inserted] = masks_.emplace(key, std::move(result));
    (void)inserted;
    return it->second;
}

bool Game::instanceCollision(const Rectangle& bounds, const InstanceDef& instance) {
    const Rectangle other = instanceBounds(instance);
    if (!CheckCollisionRecs(bounds, other)) return false;
    if(instance.object=="obj_colision" || instance.object=="obj_platform")return true;
    const auto* obj = object(instance.object);
    const auto* spr = obj ? sprite(instance.spriteOverride.empty() ? obj->sprite : instance.spriteOverride) : nullptr;
    // GameMaker stores both precise/shared (0) and precise-per-frame (4)
    // masks in this field. The car uses kind 0; treating it as a rectangle
    // made its transparent roof corners and wheel gaps lethal.
    if (!spr || (spr->collisionKind != 0 && spr->collisionKind != 4)) return true;
    const auto& bits = mask(spr->name, instance.imageIndex);
    if (bits.solid.empty() || instance.scaleX == 0 || instance.scaleY == 0) return true;
    const int left = static_cast<int>(std::floor(std::max(bounds.x, other.x)));
    const int top = static_cast<int>(std::floor(std::max(bounds.y, other.y)));
    const int right = static_cast<int>(std::ceil(std::min(bounds.x + bounds.width, other.x + other.width)));
    const int bottom = static_cast<int>(std::ceil(std::min(bounds.y + bounds.height, other.y + other.height)));
#ifdef __PSP__
    if(bits.wordsPerRow>0&&!bits.rowBits.empty()&&
       std::abs(std::abs(instance.scaleX)-1.0F)<0.0001F&&
       std::abs(std::abs(instance.scaleY)-1.0F)<0.0001F&&right>left&&bottom>top) {
        const auto sampleX=[&](int x){return static_cast<int>(std::floor((x+0.5F-instance.x)/instance.scaleX+spr->originX));};
        const auto sampleY=[&](int y){return static_cast<int>(std::floor((y+0.5F-instance.y)/instance.scaleY+spr->originY));};
        int minX=std::max(0,std::min(sampleX(left),sampleX(right-1)));
        int maxX=std::min(bits.width-1,std::max(sampleX(left),sampleX(right-1)));
        int minY=std::max(0,std::min(sampleY(top),sampleY(bottom-1)));
        int maxY=std::min(bits.height-1,std::max(sampleY(top),sampleY(bottom-1)));
        if(minX<=maxX&&minY<=maxY) {
            const int firstWord=minX/32,lastWord=maxX/32;
            const std::uint32_t firstMask=0xFFFFFFFFU<<(minX&31);
            const int lastBit=maxX&31;
            const std::uint32_t lastMask=lastBit==31?0xFFFFFFFFU:((1U<<(lastBit+1))-1U);
            for(int y=minY;y<=maxY;++y) {
                const auto* row=bits.rowBits.data()+static_cast<std::size_t>(y*bits.wordsPerRow);
                if(firstWord==lastWord) {
                    if(row[firstWord]&(firstMask&lastMask))return true;
                } else {
                    if((row[firstWord]&firstMask)||(row[lastWord]&lastMask))return true;
                    for(int word=firstWord+1;word<lastWord;++word)if(row[word])return true;
                }
            }
            return false;
        }
        return false;
    }
#endif
    for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
        const int sx = static_cast<int>(std::floor((x + 0.5F - instance.x) / instance.scaleX + spr->originX));
        const int sy = static_cast<int>(std::floor((y + 0.5F - instance.y) / instance.scaleY + spr->originY));
        if (sx >= 0 && sy >= 0 && sx < bits.width && sy < bits.height && bits.solid[sy * bits.width + sx]) return true;
    }
    return false;
}

bool Game::collides(float x, float y) {
    return collidesWithProfile(x,y,playerUsesLowProfile());
}

bool Game::collidesWithProfile(float x, float y, bool lowProfile) {
    const Rectangle bounds = playerBoundsWithProfile(x,y,lowProfile);
    const auto collidesWith=[&](const InstanceDef& instance) {
        if (instance.object == "obj_platform") {
            const Rectangle current = playerBoundsWithProfile(player_.x,player_.y,lowProfile);
            if (player_.dropThrough || current.y + current.height > instance.y) return false;
        }
        return instance.active && instanceCollision(bounds, instance);
    };
#ifdef __PSP__
    // Player movement probes run several times per simulation step. Query only
    // the structural cells touched by the small player hitbox instead of
    // scanning every wall and platform in large rooms.
    if(collisionGridColumns_>0&&collisionGridRows_>0) {
        constexpr float cellSize=128.0F;
        if(++collisionQueryStamp_==0) {
            std::fill(collisionVisitStamp_.begin(),collisionVisitStamp_.end(),0);
            collisionQueryStamp_=1;
        }
        const int left=std::clamp(static_cast<int>(std::floor(bounds.x/cellSize)),0,collisionGridColumns_-1);
        const int right=std::clamp(static_cast<int>(std::floor((bounds.x+std::max(0.0F,bounds.width-0.001F))/cellSize)),0,collisionGridColumns_-1);
        const int top=std::clamp(static_cast<int>(std::floor(bounds.y/cellSize)),0,collisionGridRows_-1);
        const int bottom=std::clamp(static_cast<int>(std::floor((bounds.y+std::max(0.0F,bounds.height-0.001F))/cellSize)),0,collisionGridRows_-1);
        for(int cy=top;cy<=bottom;++cy)for(int cx=left;cx<=right;++cx)
            for(const std::size_t index:collisionBuckets_[static_cast<std::size_t>(cy*collisionGridColumns_+cx)]) {
                if(collisionVisitStamp_[index]==collisionQueryStamp_)continue;
                collisionVisitStamp_[index]=collisionQueryStamp_;
                if(index<room_->instances.size()&&collidesWith(room_->instances[index]))return true;
            }
    }
#else
    for(const std::size_t index:collisionIndices_)
        if(index<room_->instances.size() && collidesWith(room_->instances[index]))return true;
#endif
    // Runtime effects normally are not structural, but preserve exact
    // behaviour if a room ever spawns a collision object dynamically.
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index)
        if(inherits(room_->instances[index].object,"obj_colision")&&collidesWith(room_->instances[index]))return true;
    return false;
}

bool Game::slopeBelow(float distance) {
    Rectangle probe=playerBounds(player_.x,player_.y);
    probe.height+=distance;
    for(const std::size_t index:slopeIndices_) {
        const auto& instance=room_->instances[index];
        if(instance.active&&instanceCollision(probe,instance))return true;
    }
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index) {
        const auto& instance=room_->instances[index];
        if(instance.active&&instance.object=="obj_slope"&&instanceCollision(probe,instance))return true;
    }
    return false;
}

void Game::moveAxis(float& coordinate, float amount, bool horizontal) {
    if (horizontal) {
        if (!collides(player_.x + amount, player_.y)) { coordinate += amount; return; }
        int yplus = 0;
        while (collides(player_.x + amount, player_.y - static_cast<float>(yplus)) &&
               yplus <= static_cast<int>(std::abs(amount))) ++yplus;
        if (!collides(player_.x + amount, player_.y - static_cast<float>(yplus))) {
            player_.y -= static_cast<float>(yplus);
            coordinate += amount;
            return;
        }
    }
    const float direction = sign(amount);
    float remaining = std::abs(amount);
    while (remaining > 0.0001F) {
        const float delta = direction * std::min(1.0F, remaining);
        const float nextX = horizontal ? player_.x + delta : player_.x;
        const float nextY = horizontal ? player_.y : player_.y + delta;
        if (collides(nextX, nextY)) {
            if (horizontal) player_.hsp = 0; else player_.vsp = 0;
            return;
        }
        coordinate += delta;
        remaining -= std::abs(delta);
    }
}

bool Game::touchesObject(const std::string& name) {
    const auto player = playerBounds(player_.x, player_.y);
#ifdef __PSP__
    if(name=="obj_next") {
        for(const std::size_t index:nextIndices_)
            if(index<room_->instances.size()) {
                const auto& instance=room_->instances[index];
                if(instance.active&&instanceCollision(player,instance))return true;
            }
        for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index) {
            const auto& instance=room_->instances[index];
            if(instance.active&&inherits(instance.object,name)&&instanceCollision(player,instance))return true;
        }
        return false;
    }
#endif
    return std::any_of(room_->instances.begin(), room_->instances.end(), [&](const InstanceDef& instance) {
        return instance.active && inherits(instance.object, name) && instanceCollision(player, instance);
    });
}

bool Game::touchesHazard() {
    const Rectangle player=playerBounds(player_.x,player_.y);
    const auto touches=[&](const InstanceDef& instance) {
        if (!instance.active || instance.type==4) return false;
        return instanceCollision(player,instance);
    };
    for(const std::size_t index:hazardIndices_)
        if(index<room_->instances.size()&&touches(room_->instances[index]))return true;
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index) {
        const auto& instance=room_->instances[index];
        const auto* obj=object(instance.object);
        if(obj&&(inherits(instance.object,"obj_evil")||obj->behavior!="none"||
                 instance.object=="obj_spike"||instance.object=="obj_spikeinv")&&touches(instance))return true;
    }
    return false;
}

void Game::loadRoom(std::size_t index, bool restart) {
    roomIndex_ = index % data_.roomOrder.size();
    room_ = &data_.rooms.at(data_.roomOrder[roomIndex_]);
    room_->instances = roomTemplates_.at(room_->name);
    room_->instances.reserve(room_->instances.size() + 512);
    for (auto& instance : room_->instances) {
        instance.active = true;
        if (const auto* obj = object(instance.object)) {
            instance.state = obj->state; instance.timer = obj->timer; instance.type = obj->type;
            instance.range = obj->range; instance.speed = obj->speed; instance.verticalSpeed = obj->verticalSpeed;
            instance.direction = obj->direction; instance.imageAngle = instance.rotation;
            const auto* spr = sprite(obj->sprite);
            instance.imageSpeed = obj->animationSpeed >= 0 ? obj->animationSpeed
                : instance.imageSpeed * (spr ? spr->fps / 45.0F : 0.0F);
        }
        if (instance.object == "obj_ball") {
            static const char* variants[] = {"spr_basket", "spr_ball", "spr_football"};
            instance.spriteOverride = variants[GetRandomValue(0, 2)];
        }
        if (instance.object == "obj_littlepaint")
            instance.spriteOverride="spr_lilpaint"+std::string(TextFormat("%02d",GetRandomValue(1,13)));
        if (instance.object == "obj_paint" || instance.object == "obj_paint2")
            instance.spriteOverride="spr_paint"+std::string(TextFormat("%02d",GetRandomValue(1,21)));
        if (instance.object == "obj_player_cutscene")
            instance.spriteOverride=plusMode_ ? (skin_==0 ? "spr_mib_run" : "spr_pope_run") : "spr_player_run";
        if (instance.object == "obj_candle") instance.direction = GetRandomValue(0, 1) ? 0.0F : PI;
        instance.previousX=instance.x; instance.previousY=instance.y;
    }
    hasPlayer_ = room_->hasPlayer;
    player_.x = room_->spawnX;
    player_.y = room_->spawnY;
    player_.previousX=player_.x; player_.previousY=player_.y;
    for (auto& instance : room_->instances) if (instance.object == "obj_player") instance.active = false;
    rebuildCollisionIndex();
    player_.hsp = player_.vsp = player_.animation = 0;
    player_.sprite = playerSprite("spr_player_idle");
    player_.coyote = player_.wallCoyote = player_.wallCoyoteSide = player_.jumpBuffer = player_.dashBuffer =
        player_.dashFrames = player_.slopeAnimationGrace = player_.slideGroundGrace = 0;
    player_.blinkFrames = room_->name == "rm_boss" ? 15 : 30;
    player_.deathTimer = 0;
    player_.wallLock = 0;
    player_.blinkSoundCooldown = 0;
    player_.dead = false;
    player_.sliding = player_.lowProfile = false;
    camera_.target = hasPlayer_ ? Vector2{player_.x, player_.y}
                                : Vector2{room_->viewWidth / 2.0F, room_->viewHeight / 2.0F};
    previousCameraTarget_=camera_.target;
    rebuildGraphicIndex();
    sceneTimer_ = 0;
    roomStepCounter_ = 0;
    if (room_->name == "rm_end") {
        trueEnding_ = chocolates_ > 14;
        endingAnim_ = 0;
    }
    transitioning_ = false;
    transitionTimer_ = 0;
    roomFadeMax_ = 10;
    roomFadeFrames_ = hasPlayer_ ? roomFadeMax_ : 0;
    titleFrames_ = hasPlayer_ && room_->name != "rm_boss" ? 50 : 0;
    if (!restart) roomStartChocolates_ = chocolates_;
    // A GameMaker room restart recreates obj_boss and reruns Create_0.gml.
    // Reset every fight state on both first entry and death/restart.
    if (room_->name == "rm_boss") {
        bossLife_ = 317.892298784F;
        bossState_ = bossTimer_ = bossAttackTimer_ = bossEyeTimer_ = bossDeathTimer_ = 0;
        bossTiming_ = 60;
    }
    setRoomMusic();
    preloadRoomAssets();
    preloadNextRoomMusic();
}

void Game::preloadRoomAssets() {
    const auto warmSprite=[&](const std::string& name) {
        if(name.empty())return;
        auto& loaded=texture(name);
#ifdef __PSP__
        for(const auto& frame:loaded.frames)PreloadTexture(frame);
#endif
    };
    // Thousands of legacy tiles often reference the same tileset. Warm every
    // sprite once instead of hashing the same name once per tile during a room
    // transition.
    std::unordered_set<std::string> roomSprites;
    roomSprites.reserve(room_->backgrounds.size()+room_->instances.size()+16);
    for (const auto& background:room_->backgrounds) if(!background.sprite.empty())roomSprites.insert(background.sprite);
#if !defined(__3DS__) && !defined(__PSP__)
    for (const auto& graphic:room_->graphics) if(!graphic.sprite.empty())roomSprites.insert(graphic.sprite);
#else
    // Warm only tiles in and just beyond the initial camera. Other sheets are
    // acquired lazily as the camera approaches them, avoiding needless atlas
    // I/O and immediate LRU eviction while a large room is loading.
    for (const auto& graphic:room_->graphics) {
        if(graphic.sprite.empty())continue;
        if(std::abs(graphic.x-camera_.target.x)<=kViewWidth/2.0F+64.0F &&
           std::abs(graphic.y-camera_.target.y)<=kViewHeight/2.0F+64.0F)
            roomSprites.insert(graphic.sprite);
    }
#endif
    for (const auto& instance:room_->instances) {
#ifdef __3DS__
        if(std::abs(instance.x-camera_.target.x)>kViewWidth/2.0F+96.0F ||
           std::abs(instance.y-camera_.target.y)>kViewHeight/2.0F+96.0F)
            continue;
#endif
        const auto* obj=object(instance.object);
        if (!obj) continue;
        const std::string& spriteName=instance.spriteOverride.empty()?obj->sprite:instance.spriteOverride;
        if(!spriteName.empty())roomSprites.insert(spriteName);
        if (!obj->spawnObject.empty()) {
            if (const auto* spawned=object(obj->spawnObject);spawned&&!spawned->sprite.empty())roomSprites.insert(spawned->sprite);
        }
#if !defined(__3DS__) && !defined(__PSP__)
        // The desktop has ample I/O bandwidth and can warm precise masks. On
        // 3DS these are loaded lazily only when their AABB is actually touched,
        // avoiding work for enemies and collision art far beyond the spawn.
        if (instance.active && inherits(instance.object,"obj_colision") && !spriteName.empty()) {
            if(const auto* definition=sprite(spriteName);definition&&
               (definition->collisionKind==0||definition->collisionKind==4))
                (void)mask(spriteName,instance.imageIndex);
        }
#endif
    }
    for(const auto& name:roomSprites)warmSprite(name);
    if (room_->name=="rm_boss" || room_->name=="rm_boss_scene") {
        static constexpr const char* bossSprites[]={"spr_boss_idle","spr_boss_hit","spr_boss_eyeatk",
            "spr_boss_eyegrown","spr_boss_death","spr_trash_ball","spr_eyel","spr_eyer","spr_car"};
        for(const char* name:bossSprites) {
            warmSprite(name);
            const auto* definition=sprite(name);
            if(definition && (definition->collisionKind==0 || definition->collisionKind==4))
#if !defined(__3DS__) && !defined(__PSP__)
                (void)mask(name,0);
#else
                (void)definition;
#endif
        }
    }
    if (hasPlayer_) {
        static constexpr const char* states[]={"idle","run","jump","fall","walljump","crouch","slide","blink","dash","death"};
        for (const char* state:states) warmSprite(playerSprite(std::string("spr_player_")+state));
        static constexpr const char* common[]={"spr_effect","spr_chocolat","spr_cavera","spr_popup","spr_button",
            "spr_jumpsmoke","spr_wallsmoke","spr_trail","spr_trailslide","spr_blink","spr_sparkle"};
        for (const char* name:common) warmSprite(name);
        if (plusMode_ && skin_>=0) {
            const std::string prefix=skin_==0?"spr_mib_":"spr_pope_";
            warmSprite(prefix+"trail"); warmSprite(prefix+"trailslide");
        }
    }
    static constexpr const char* ui[]={"spr_arrow","spr_cross","spr_brazil","spr_usa","spr_pause",
        "spr_longarrow","spr_sound","spr_stopwatch","spr_bossbar_back","spr_bossbar","spr_end","spr_trueend"};
    for (const char* name:ui) warmSprite(name);
    static const std::unordered_map<std::string,std::string> titles{
        {"rm_0","spr_en_bedroom"},{"rm_1","spr_en_stairs"},{"rm_2","spr_en_kitchen"},
        {"rm_3","spr_en_living"},{"rm_4","spr_en_corridor"},{"rm_5","spr_en_garage"}};
    if (const auto found=titles.find(room_->name);found!=titles.end()) {
        warmSprite(found->second);
        static const std::unordered_map<std::string,std::string> portuguese{
            {"rm_0","spr_title_bedroom"},{"rm_1","spr_title_stairs"},{"rm_2","spr_title_kitchen"},
            {"rm_3","spr_title_living"},{"rm_4","spr_title_corridor"},{"rm_5","spr_title_garage"}};
        warmSprite(portuguese.at(room_->name));
    }
    drawScratch_.clear();
#if defined(__3DS__) || defined(__PSP__)
    // Only visible room graphics enter the 3DS draw list. Keeping this buffer
    // close to the visible workload avoids allocating space for several
    // thousand off-screen legacy tiles during every room transition.
    drawScratch_.reserve(room_->backgrounds.size()+room_->instances.capacity()+768);
#else
    drawScratch_.reserve(room_->backgrounds.size()+room_->graphics.size()+room_->instances.capacity()+1);
#endif
    rebuildDrawList();
#ifdef __PSP__
    drawListValid_=false;
#endif
    sparkleScratch_.reserve(8);
    spawnScratch_.reserve(32);
    simulationWorkIndices_.reserve(room_->instances.capacity());
}

void Game::rebuildGraphicIndex() {
    graphicBounds_.clear();
    graphicBuckets_.clear();
    graphicVisitStamp_.clear();
    visibleGraphicScratch_.clear();
#if defined(__3DS__) || defined(__PSP__)
    constexpr float cellSize=128.0F;
    graphicGridColumns_=std::max(1,static_cast<int>(std::ceil(room_->width/cellSize)));
    graphicGridRows_=std::max(1,static_cast<int>(std::ceil(room_->height/cellSize)));
    graphicBounds_.resize(room_->graphics.size());
    graphicBuckets_.resize(static_cast<std::size_t>(graphicGridColumns_*graphicGridRows_));
    graphicVisitStamp_.resize(room_->graphics.size());
#ifdef __PSP__
    preparedGraphics_.clear();
    preparedGraphics_.resize(room_->graphics.size());
    std::unordered_map<std::string,Texture2D> preparedSheets;
    preparedSheets.reserve(8);
#endif
    visibleGraphicScratch_.reserve(512);
    for(std::size_t index=0;index<room_->graphics.size();++index) {
        const auto& graphic=room_->graphics[index];
        const auto* definition=sprite(graphic.sprite);
        const float rawWidth=std::abs(graphic.width)!=0.0F?std::abs(graphic.width)
            : static_cast<float>(definition?definition->width:0);
        const float rawHeight=std::abs(graphic.height)!=0.0F?std::abs(graphic.height)
            : static_cast<float>(definition?definition->height:0);
        const float width=rawWidth*std::abs(graphic.scaleX);
        const float height=rawHeight*std::abs(graphic.scaleY);
        Rectangle bounds{};
        if(std::abs(graphic.rotation)<0.001F) bounds={graphic.x,graphic.y,width,height};
        else {
            const float radius=std::hypot(width,height);
            bounds={graphic.x-radius,graphic.y-radius,radius*2.0F,radius*2.0F};
        }
        graphicBounds_[index]=bounds;
#ifdef __PSP__
        Texture2D image{};
        if(!graphic.sprite.empty()) {
            const auto found=preparedSheets.find(graphic.sprite);
            if(found!=preparedSheets.end())image=found->second;
            else {
                auto& frames=texture(graphic.sprite).frames;
                if(!frames.empty())image=frames[0];
                preparedSheets.emplace(graphic.sprite,image);
            }
        }
        if(image.id) {
            const float sourceWidth=std::abs(graphic.width)!=0.0F?std::abs(graphic.width):static_cast<float>(image.width);
            const float sourceHeight=std::abs(graphic.height)!=0.0F?std::abs(graphic.height):static_cast<float>(image.height);
            const bool flipX=(graphic.width<0)!=(graphic.scaleX<0);
            const bool flipY=(graphic.height<0)!=(graphic.scaleY<0);
            constexpr float inset=0.01F;
            Rectangle source{graphic.u0+inset,graphic.v0+inset,sourceWidth-inset*2,sourceHeight-inset*2};
            if(flipX)source.width=-source.width;
            if(flipY)source.height=-source.height;
            preparedGraphics_[index]={image,source,
                {graphic.x,graphic.y,sourceWidth*std::abs(graphic.scaleX),sourceHeight*std::abs(graphic.scaleY)},
                graphic.rotation,gmColour(graphic.colour)};
        }
#endif
        const int left=std::clamp(static_cast<int>(std::floor(bounds.x/cellSize)),0,graphicGridColumns_-1);
        const int right=std::clamp(static_cast<int>(std::floor((bounds.x+std::max(0.0F,bounds.width-0.001F))/cellSize)),0,graphicGridColumns_-1);
        const int top=std::clamp(static_cast<int>(std::floor(bounds.y/cellSize)),0,graphicGridRows_-1);
        const int bottom=std::clamp(static_cast<int>(std::floor((bounds.y+std::max(0.0F,bounds.height-0.001F))/cellSize)),0,graphicGridRows_-1);
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)
            graphicBuckets_[static_cast<std::size_t>(y*graphicGridColumns_+x)].push_back(index);
    }
#endif
}

void Game::rebuildCollisionIndex() {
    collisionIndices_.clear();
    slopeIndices_.clear();
    simulationIndices_.clear();
    interactionIndices_.clear();
    nextIndices_.clear();
    animationIndices_.clear();
    hazardIndices_.clear();
    lightIndices_.clear();
    roomLighting_=false;
    collisionIndices_.reserve(room_->instances.size());
    for(std::size_t i=0;i<room_->instances.size();++i) {
        const auto& instance=room_->instances[i];
        const auto* obj=object(instance.object);
        if(inherits(instance.object,"obj_colision"))collisionIndices_.push_back(i);
        if(instance.object=="obj_slope")slopeIndices_.push_back(i);
        const bool pickup=instance.object=="obj_chocolate" || instance.object=="obj_lollipop" ||
            instance.object=="obj_pocket" || instance.object=="obj_sock" || instance.object=="obj_bible" ||
            instance.object=="obj_cruz" || instance.object=="obj_oil" || instance.object=="obj_vela" ||
            instance.object=="obj_water";
        const bool effect=instance.object=="obj_trail" || instance.object=="obj_trailslide" ||
            instance.object=="obj_trailball" || instance.object=="obj_trailchair" || instance.object=="obj_blink" ||
            instance.object=="obj_jumpsmoke" || instance.object=="obj_wallsmoke" || instance.object=="obj_sparkle";
        if(pickup || effect || (obj && obj->behavior!="none"))simulationIndices_.push_back(i);
        if(pickup||instance.object.starts_with("obj_placa"))interactionIndices_.push_back(i);
        if(inherits(instance.object,"obj_next"))nextIndices_.push_back(i);
        // Pickups and bosses can start animating after their index is built.
        if(obj) {
            const auto* visual=sprite(instance.spriteOverride.empty()?obj->sprite:instance.spriteOverride);
            if(visual && visual->frames.size()>1)animationIndices_.push_back(i);
        }
        if(instance.type!=4 && obj && (inherits(instance.object,"obj_evil") || obj->behavior!="none" ||
            instance.object=="obj_spike" || instance.object=="obj_spikeinv"))hazardIndices_.push_back(i);
        if(instance.active&&instance.object=="obj_light")roomLighting_=true;
        if(instance.object=="obj_lighting" || instance.object=="obj_lighthouse" ||
           instance.object=="obj_bestlight" || instance.object=="obj_candle" ||
           instance.object=="obj_fireball" || instance.object=="obj_trash_ball" ||
           instance.object=="obj_chocolate" || instance.object=="obj_lollipop" ||
           instance.object=="obj_pocket" || instance.object=="obj_sock" ||
           instance.object=="obj_bible" || instance.object=="obj_cruz" ||
           instance.object=="obj_oil" || instance.object=="obj_vela" ||
           instance.object=="obj_water" || instance.object=="obj_boss" ||
           instance.object=="obj_wallamp" || instance.object=="obj_wallamp2" ||
           instance.object=="obj_window" || instance.object=="obj_window2")
            lightIndices_.push_back(i);
    }
    indexedInstanceCount_=room_->instances.size();
#ifdef __PSP__
    constexpr float cellSize=128.0F;
    collisionGridColumns_=std::max(1,static_cast<int>(std::ceil(room_->width/cellSize)));
    collisionGridRows_=std::max(1,static_cast<int>(std::ceil(room_->height/cellSize)));
    collisionBuckets_.clear();
    collisionBuckets_.resize(static_cast<std::size_t>(collisionGridColumns_*collisionGridRows_));
    collisionVisitStamp_.assign(room_->instances.size(),0);
    collisionQueryStamp_=1;
    for(const std::size_t index:collisionIndices_) {
        const Rectangle bounds=instanceBounds(room_->instances[index]);
        const int left=std::clamp(static_cast<int>(std::floor(bounds.x/cellSize)),0,collisionGridColumns_-1);
        const int right=std::clamp(static_cast<int>(std::floor((bounds.x+std::max(0.0F,bounds.width-0.001F))/cellSize)),0,collisionGridColumns_-1);
        const int top=std::clamp(static_cast<int>(std::floor(bounds.y/cellSize)),0,collisionGridRows_-1);
        const int bottom=std::clamp(static_cast<int>(std::floor((bounds.y+std::max(0.0F,bounds.height-0.001F))/cellSize)),0,collisionGridRows_-1);
        for(int cy=top;cy<=bottom;++cy)for(int cx=left;cx<=right;++cx)
            collisionBuckets_[static_cast<std::size_t>(cy*collisionGridColumns_+cx)].push_back(index);
    }
#endif
}

void Game::rebuildDrawList() {
    drawScratch_.clear();
#if defined(__3DS__) || defined(__PSP__)
    const float viewLeft=camera_.target.x-kViewWidth/2.0F;
    const float viewTop=camera_.target.y-kViewHeight/2.0F;
#ifdef __PSP__
    // The PSP reuses the sorted list for a few frames. Keep a guard band so
    // camera motion never exposes an unloaded tile or moving instance.
    constexpr float drawGuard=48.0F;
    const Rectangle visibleGraphics{viewLeft-drawGuard,viewTop-drawGuard,
                                    kViewWidth+drawGuard*2,kViewHeight+drawGuard*2};
#else
    const Rectangle visibleGraphics{viewLeft,viewTop,kViewWidth,kViewHeight};
#endif
#endif
    for (std::size_t i=0;i<room_->backgrounds.size();++i)
        drawScratch_.push_back({room_->backgrounds[i].depth,DrawKind::Background,i});
    visibleGraphicScratch_.clear();
#if defined(__3DS__) || defined(__PSP__)
    constexpr float cellSize=128.0F;
    if(++graphicQueryStamp_==0) {
        std::fill(graphicVisitStamp_.begin(),graphicVisitStamp_.end(),0);
        graphicQueryStamp_=1;
    }
    const int left=std::clamp(static_cast<int>(std::floor(visibleGraphics.x/cellSize)),0,graphicGridColumns_-1);
    const int right=std::clamp(static_cast<int>(std::floor((visibleGraphics.x+visibleGraphics.width)/cellSize)),0,graphicGridColumns_-1);
    const int top=std::clamp(static_cast<int>(std::floor(visibleGraphics.y/cellSize)),0,graphicGridRows_-1);
    const int bottom=std::clamp(static_cast<int>(std::floor((visibleGraphics.y+visibleGraphics.height)/cellSize)),0,graphicGridRows_-1);
    for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)
        for(const std::size_t index:graphicBuckets_[static_cast<std::size_t>(y*graphicGridColumns_+x)]) {
            if(graphicVisitStamp_[index]==graphicQueryStamp_)continue;
            graphicVisitStamp_[index]=graphicQueryStamp_;
            if(CheckCollisionRecs(graphicBounds_[index],visibleGraphics))visibleGraphicScratch_.push_back(index);
        }
    std::sort(visibleGraphicScratch_.begin(),visibleGraphicScratch_.end());
#else
    visibleGraphicScratch_.resize(room_->graphics.size());
    for(std::size_t i=0;i<visibleGraphicScratch_.size();++i)visibleGraphicScratch_[i]=i;
#endif
    for(const std::size_t i:visibleGraphicScratch_)
        drawScratch_.push_back({room_->graphics[i].depth,DrawKind::Graphic,i});
    for (std::size_t i=0;i<room_->instances.size();++i) if (room_->instances[i].active) {
#if defined(__3DS__) || defined(__PSP__)
        const auto& instance=room_->instances[i];
        const auto* obj=object(instance.object);
        if(!obj || !obj->visible || instance.object=="obj_title" || instance.object=="obj_platform")continue;
        const std::string& spriteName=instance.spriteOverride.empty()?obj->sprite:instance.spriteOverride;
        const auto* visual=sprite(spriteName);
        if(!visual)continue;
        float x=instance.x,y=instance.y;
        if(
#ifdef __3DS__
            room_->name=="rm_credits"&&instance.object=="obj_guris"
#else
            false
#endif
        ) {
            x=kViewWidth/2.0F;y=kViewHeight/2.0F;
        }
        const float width=visual->width*std::abs(instance.scaleX);
        const float height=visual->height*std::abs(instance.scaleY);
        const float originX=(instance.scaleX<0?visual->width-visual->originX:visual->originX)*std::abs(instance.scaleX);
        const float originY=(instance.scaleY<0?visual->height-visual->originY:visual->originY)*std::abs(instance.scaleY);
        const float radius=std::hypot(width,height);
        const Rectangle bounds=std::abs(instance.imageAngle)<0.001F
            ? Rectangle{x-originX,y-originY,width,height}
            : Rectangle{x-radius,y-radius,radius*2.0F,radius*2.0F};
        if(!CheckCollisionRecs(bounds,visibleGraphics))continue;
#endif
        drawScratch_.push_back({room_->instances[i].depth,DrawKind::Instance,i});
    }
    if (hasPlayer_ && !transitioning_) drawScratch_.push_back({-5,DrawKind::Player,0});
    // Explicit tie-breaking preserves the old stable insertion order without
    // stable_sort's temporary allocation and extra moves on handheld CPUs.
    std::sort(drawScratch_.begin(),drawScratch_.end(),[](const DrawItem& a,const DrawItem& b){
        if(a.depth!=b.depth)return a.depth>b.depth;
        if(a.kind!=b.kind)return a.kind<b.kind;
        return a.index<b.index;
    });
}

void Game::updatePlayer() {
    if (player_.dead) {
        player_.sprite = playerSprite("spr_player_death");
        player_.animation = std::min(16.99F, player_.animation + 0.20F);
        player_.vsp = std::min(10.0F, player_.vsp + kGravity);
        moveAxis(player_.y, player_.vsp, false);
        if (player_.deathTimer == 85) ++deaths_;
        if (++player_.deathTimer > 145) {
            chocolates_ = roomStartChocolates_;
            loadRoom(roomIndex_, true);
            player_.x = room_->respawnX;
            player_.y = room_->respawnY;
        }
        return;
    }
    const float axis = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
    const float verticalAxis = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
    const bool left = qaHorizontal_<0 || IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) || axis < -0.25F ||
                      IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    const bool right = qaHorizontal_>0 || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) || axis > 0.25F ||
                       IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    const bool down = qaDown_ || verticalAxis > 0.25F || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ||
                      IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)
#ifndef __3DS__
                      || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1)
#endif
                      ;
    const bool up = IsKeyDown(KEY_W)
#if !defined(__3DS__) && !defined(__PSP__)
                    || IsKeyDown(KEY_UP) ||
                    IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP) ||
                    IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
                    IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_UP) ||
                    IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
#endif
                    ;
    const bool jumpPressed = (qaJumpPeriod_>0 && stepCounter_%qaJumpPeriod_==0) ||
                             input_.jump;
    const bool qaJumpHeld=qaJumpPeriod_>0 && stepCounter_%qaJumpPeriod_<6;
    const bool jumpHeld = qaJumpHeld || IsKeyDown(KEY_SPACE) ||
#ifdef __3DS__
                          IsGamepadButtonDown(0,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) ||
                          IsGamepadButtonDown(0,GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
#else
                          IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
#endif
    player_.dropThrough = down;
    activeSign_ = 0;

    player_.grounded = collides(player_.x, player_.y + 1);
    if (player_.grounded) {
        player_.coyote = 6;
        player_.dashReady = true;
    } else if (player_.coyote > 0) --player_.coyote;
    // Match buffer_max=4 from obj_player/Create; pulses are still latched by
    // the render loop so a short press cannot disappear between fixed steps.
    // Input pulses are latched by the render loop, so a short press cannot be
    // lost between two fixed simulation steps.
    if (jumpPressed) player_.jumpBuffer = 4;
    else if (player_.jumpBuffer > 0) --player_.jumpBuffer;
    // Retain a dash press for five simulation frames. Holding the button keeps
    // the original behavior; the counter only makes short early taps reliable.
    if (input_.dash) player_.dashBuffer = 5;
    else if (player_.dashBuffer > 0) --player_.dashBuffer;

    const float inputMove = static_cast<float>(right - left);
    const bool slideContact=player_.grounded ||
        (player_.sliding && player_.slideGroundGrace>0);
    // Enter the physical slide state before horizontal collision is evaluated.
    // Previously the sprite changed to slide only after moveAxis(), so the
    // entrance step of a low passage could still be tested as a standing body.
    if (down && slideContact && haveSlide_ &&
        (player_.sliding || player_.hsp != 0 || inputMove != 0)) player_.sliding=true;
    if (player_.wallLock > 0) {
        player_.hsp = player_.forcedMove * 4.0F;
        --player_.wallLock;
    } else if (!player_.sliding) player_.hsp = inputMove * 4.0F;
    // Once a slide starts, its short mask remains authoritative for the whole
    // slide. Ground seams and state-transition ordering must not restore the
    // standing mask while Elli is still sliding.
    const bool wantsLowProfile=player_.sliding ||
        (down&&slideContact&&(player_.hsp==0||haveSlide_));
    if (wantsLowProfile) player_.lowProfile=true;
    else if (player_.lowProfile && !collidesWithProfile(player_.x,player_.y,false)) player_.lowProfile=false;
    if (player_.lowProfile && !down && collidesWithProfile(player_.x,player_.y,false)) player_.hsp=0;
    if (player_.hsp != 0) player_.facing = sign(player_.hsp);

    if ((up || player_.dashBuffer > 0) && player_.hsp != 0 && !player_.grounded && player_.dashReady && haveDash_) {
        player_.dashFrames = 10;
        player_.dashReady = false;
        player_.dashBuffer = 0;
    }
    if (player_.dashFrames > 0) {
        auto trail=makeInstance("obj_trail",player_.x,player_.y);
        trail.imageIndex=player_.animation; trail.scaleX=player_.facing;
        if (plusMode_) trail.spriteOverride=skin_==0 ? "spr_mib_trail" : "spr_pope_trail";
        room_->instances.push_back(std::move(trail));
        moveAxis(player_.x, player_.hsp * 2.5F, true);
        player_.vsp = 0;
        --player_.dashFrames;
        player_.sprite = playerSprite("spr_player_dash");
    } else {
        const bool wallRight = collides(player_.x + 1, player_.y);
        const bool wallLeft = collides(player_.x - 1, player_.y);
        if (!player_.grounded && room_->name != "rm_boss") {
            if (wallRight) { player_.wallCoyote=6; player_.wallCoyoteSide=1; }
            else if (wallLeft) { player_.wallCoyote=6; player_.wallCoyoteSide=-1; }
            else if (player_.wallCoyote>0) --player_.wallCoyote;
        } else {
            player_.wallCoyote=0;player_.wallCoyoteSide=0;
        }
        const bool wallJumpAvailable=!player_.grounded && player_.wallCoyote>0 &&
            ((player_.wallCoyoteSide>0&&right)||(player_.wallCoyoteSide<0&&left));
        // In the original Step event gravity runs before the jump assignment.
        player_.vsp = std::min(10.0F, player_.vsp + kGravity);
        if (player_.jumpBuffer>0 && wallJumpAvailable) {
            player_.vsp = -10.0F;
            player_.forcedMove = player_.wallCoyoteSide>0 ? -1.0F : 1.0F;
            player_.wallLock = 10;
            player_.jumpBuffer=player_.wallCoyote=0;
            auto smoke=makeInstance("obj_wallsmoke",player_.x,player_.y);
            smoke.scaleX=-player_.forcedMove;
            room_->instances.push_back(std::move(smoke));
            playSound("sfx_jump", 0.17F);
        } else if (player_.jumpBuffer > 0 && player_.coyote > 0) {
            player_.vsp = -10.0F;
            player_.jumpBuffer = player_.coyote = 0;
            room_->instances.push_back(makeInstance("obj_jumpsmoke",player_.x,player_.y));
            playSound("sfx_jump", 0.17F);
        }
        if (!jumpHeld && player_.vsp < 0) player_.vsp = 0;
        if (!player_.grounded && player_.vsp > 0 && room_->name != "rm_boss" &&
            ((wallRight && right) || (wallLeft && left))) player_.vsp /= 1.75F;
        moveAxis(player_.x, player_.hsp, true);
        moveAxis(player_.y, player_.vsp, false);
        player_.grounded = collides(player_.x, player_.y + 1);
        const bool slopeNear=player_.hsp!=0&&player_.vsp>=0&&slopeBelow(40);
        if (slopeNear) player_.slopeAnimationGrace=4;
        else if (player_.slopeAnimationGrace>0) --player_.slopeAnimationGrace;
        if (down && player_.grounded && player_.hsp == 0) {
            if (haveBlink_ && player_.blinkFrames > 0) { player_.sprite = playerSprite("spr_player_blink"); --player_.blinkFrames; }
            else player_.sprite = playerSprite("spr_player_crouch");
        }
        else if (player_.sliding && player_.hsp != 0) {
            player_.sprite = playerSprite("spr_player_slide");
            auto trail=makeInstance("obj_trailslide",player_.x,player_.y);
            trail.imageIndex=player_.animation; trail.scaleX=player_.facing;
            if (plusMode_) trail.spriteOverride=skin_==0 ? "spr_mib_trailslide" : "spr_pope_trailslide";
            room_->instances.push_back(std::move(trail));
            if (player_.hsp < 0) player_.hsp = std::min(0.0F, player_.hsp + 0.05F);
            else player_.hsp = std::max(0.0F, player_.hsp - 0.05F);
        }
        else if (!player_.grounded && player_.vsp > 0 && room_->name != "rm_boss" &&
                 ((collides(player_.x+1,player_.y) && right) ||
                  (collides(player_.x-1,player_.y) && left))) {
            player_.sprite = playerSprite("spr_player_walljump");
            player_.facing = collides(player_.x+1,player_.y) ? 1.0F : -1.0F;
        }
        else if (!player_.grounded && player_.vsp < 0) player_.sprite = playerSprite("spr_player_jump");
        else if (!player_.grounded && player_.vsp >= 0 && player_.hsp != 0 && player_.slopeAnimationGrace>0)
            player_.sprite = playerSprite("spr_player_run");
        else if (!player_.grounded && player_.vsp >= 0) player_.sprite = playerSprite("spr_player_fall");
        else if (player_.lowProfile) player_.sprite = playerSprite("spr_player_crouch");
        else if (player_.hsp != 0) player_.sprite = playerSprite("spr_player_run");
        else player_.sprite = playerSprite("spr_player_idle");
        if (player_.grounded) player_.slideGroundGrace=2;
        else if (player_.slideGroundGrace>0) --player_.slideGroundGrace;
        if (!down || (!player_.grounded && player_.slideGroundGrace==0)) player_.sliding = false;
        if (!down) player_.blinkFrames = room_->name == "rm_boss" ? 15 : 30;
    }

    player_.animation += (player_.sprite.ends_with("_run") ? 0.30F : 0.20F);
    sparkleScratch_.clear();
    const Rectangle interactionBounds=playerBounds(player_.x,player_.y);
    const auto interact=[&](InstanceDef& instance) {
        if(!instance.active)return;
        const bool mainPickup=instance.object=="obj_chocolate" || instance.object=="obj_lollipop" ||
                              instance.object=="obj_pocket" || instance.object=="obj_sock";
        const bool plusPickup=instance.object=="obj_bible" || instance.object=="obj_cruz" ||
                              instance.object=="obj_oil" || instance.object=="obj_vela" || instance.object=="obj_water";
        const bool sign=instance.object.starts_with("obj_placa");
        if(plusMode_ && mainPickup) {
            instance.active = false;
            return;
        }
        if(plusPickup && !plusMode_) {
            instance.active=false;
            return;
        }
        if(!mainPickup && !plusPickup && !sign)return;
        const Rectangle objectBounds=instanceBounds(instance);
        const bool pickupTouch=instance.state==0 && CheckCollisionRecs(interactionBounds,objectBounds);
        if (pickupTouch && instance.object == "obj_chocolate") {
            instance.state = 1; instance.timer = 0; instance.imageSpeed = 0;
            sparkleScratch_.push_back({instance.x,instance.y});
            playSound("sfx_coin", 0.5F);
        }
        const bool touching = CheckCollisionRecs(interactionBounds,objectBounds);
        if (touching && instance.state == 0 &&
            (instance.object == "obj_lollipop" || instance.object == "obj_pocket" || instance.object == "obj_sock")) {
            instance.state = 1; instance.timer = 0; instance.imageSpeed = 0;
            // Grant immediately so the room exit cannot discard the upgrade
            // while its original pickup fade is still playing.
            if (instance.object == "obj_lollipop") haveDash_ = true;
            else if (instance.object == "obj_pocket") haveBlink_ = true;
            else if (instance.object == "obj_sock") haveSlide_ = true;
            sparkleScratch_.push_back({instance.x,instance.y});
            playSound("sfx_coin", 0.5F);
        }
        if (plusPickup && touching && instance.state == 0) {
            instance.state = 1; instance.timer = 0; instance.imageSpeed = 0;
            sparkleScratch_.push_back({instance.x,instance.y});
            playSound("sfx_coin", 0.5F);
        }
        if (sign) {
            const bool signTouch = instanceCollision(interactionBounds,instance);
            instance.imageIndex = signTouch ? 1.0F : 0.0F;
            if (signTouch && instance.object.size() > 9) activeSign_ = instance.object.back()-'0';
        }
    };
    for(const std::size_t index:interactionIndices_)
        if(index<room_->instances.size())interact(room_->instances[index]);
    // Runtime objects normally contain only particles and attacks. Preserve
    // support for a dynamically-created pickup/sign without scanning all of
    // the static room furniture every simulation step.
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index) {
        auto& instance=room_->instances[index];
        if(instance.object.starts_with("obj_placa")||instance.object=="obj_chocolate"||
           instance.object=="obj_lollipop"||instance.object=="obj_pocket"||instance.object=="obj_sock"||
           instance.object=="obj_bible"||instance.object=="obj_cruz"||instance.object=="obj_oil"||
           instance.object=="obj_vela"||instance.object=="obj_water")interact(instance);
    }
    // The original sign Draw event fades the prompt in by 0.1 per 45 Hz step
    // and immediately resets it while S/down is held.
    if (activeSign_ > 0 && !down) signPromptAlpha_ = std::min(1.0F, signPromptAlpha_ + 0.1F);
    else signPromptAlpha_ = 0.0F;
    for (const auto& position:sparkleScratch_) {
        auto sparkle=makeInstance("obj_sparkle",position.x,position.y);
        if (plusMode_) sparkle.spriteOverride="spr_sparkle_plus";
        sparkle.imageSpeed=0.2F;
        room_->instances.push_back(std::move(sparkle));
    }
    const bool blinking = player_.sprite.ends_with("_blink");
    if (player_.blinkSoundCooldown > 0) --player_.blinkSoundCooldown;
    const bool touchingHazard=touchesHazard();
    if (blinking && touchingHazard && player_.blinkSoundCooldown == 0) {
        room_->instances.push_back(makeInstance("obj_blink",player_.x,player_.y));
        playSound("sfx_blink", 0.5F);
        player_.blinkSoundCooldown = 12;
    }
    if (!blinking && touchingHazard) {
        player_.dead = true;
        player_.lowProfile = player_.sliding = false;
        player_.deathTimer = 0;
        player_.animation = 0;
        playSound("sfx_hurt");
    }
    if (touchesObject("obj_next") && roomIndex_ + 1 < data_.roomOrder.size()) {
        transitioning_ = true;
        transitionTimer_ = 0;
    }
}

void Game::updateInstances() {
    if (!hasPlayer_) return;
#if defined(__3DS__) || defined(__PSP__)
    // Dead trails/effects do not participate in simulation or drawing. Batch
    // their physical removal to avoid shifting the whole room vector every
    // OLD 3DS frame while sliding or dashing.
    const bool compactInstances=(stepCounter_&7U)==0;
#else
    const bool compactInstances=true;
#endif
    if(compactInstances) {
        const std::size_t oldSize=room_->instances.size();
        room_->instances.erase(std::remove_if(room_->instances.begin(),room_->instances.end(),
            [](const InstanceDef& i){return !i.active;}),room_->instances.end());
        if(room_->instances.size()!=oldSize)rebuildCollisionIndex();
    }
    const Rectangle playerHitbox=playerBounds(player_.x,player_.y);
    const auto distanceSquared = [&](const InstanceDef& i) {
        // GameMaker's distance_to_object measures the shortest gap between the
        // two collision boxes, not the distance between instance origins.
        const Rectangle enemy=instanceBounds(i);
        const float dx=std::max({enemy.x-(playerHitbox.x+playerHitbox.width),
                                playerHitbox.x-(enemy.x+enemy.width),0.0F});
        const float dy=std::max({enemy.y-(playerHitbox.y+playerHitbox.height),
                                playerHitbox.y-(enemy.y+enemy.height),0.0F});
        return dx*dx+dy*dy;
    };
    spawnScratch_.clear();
    simulationWorkIndices_.clear();
    simulationWorkIndices_.insert(simulationWorkIndices_.end(),simulationIndices_.begin(),simulationIndices_.end());
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index)
        simulationWorkIndices_.push_back(index);
    for (const std::size_t instanceIndex:simulationWorkIndices_) {
        if(instanceIndex>=room_->instances.size())continue;
        auto& i=room_->instances[instanceIndex];
        if (!i.active) continue;
        const auto* obj = object(i.object);
        if (!obj) continue;
        if (obj->behavior!="none") {
            if (i.type==0 || i.type==2) i.depth=-1;
            else if (i.type==1) i.depth=-2;
            else if (i.type==3) i.depth=0;
            else if (i.type==4) i.depth=1;
        }
        const bool fadingTrail=i.object=="obj_trail" || i.object=="obj_trailslide" ||
                               i.object=="obj_trailball" || i.object=="obj_trailchair";
        if (fadingTrail) {
            const unsigned alpha=(i.colour>>24)&0xffU;
            const unsigned decrement=i.object=="obj_trail" ? 26U : (i.object=="obj_trailslide" ? 51U : 102U);
            const unsigned next=alpha>decrement ? alpha-decrement : 0;
            i.colour=(i.colour&0x00ffffffU)|(next<<24);
            if (next<26) i.active=false;
            continue;
        }
        const bool animatedEffect=i.object=="obj_blink" || i.object=="obj_jumpsmoke" ||
                                  i.object=="obj_wallsmoke" || i.object=="obj_sparkle";
        if (animatedEffect) {
            const auto* effectSprite=sprite(i.spriteOverride.empty()?obj->sprite:i.spriteOverride);
            if (effectSprite && i.imageIndex>=effectSprite->frames.size()) i.active=false;
            continue;
        }
        const bool mainPickup = i.object == "obj_chocolate" || i.object == "obj_lollipop" ||
                                i.object == "obj_pocket" || i.object == "obj_sock";
        const bool plusPickup = i.object == "obj_bible" || i.object == "obj_cruz" || i.object == "obj_oil" ||
                                i.object == "obj_vela" || i.object == "obj_water";
        if ((mainPickup || plusPickup) && i.state==0) {
            ++i.timer;
            i.imageAngle += i.direction==0 ? 0.5F : -0.5F;
            if (i.imageAngle>=8) i.direction=1;
            else if (i.imageAngle<=-8) i.direction=0;
            if (i.timer>60) i.imageSpeed=0.2F;
            const auto* pickupSprite=sprite(obj->sprite);
            if (pickupSprite && i.imageIndex>=pickupSprite->frames.size()) {
                i.imageIndex=0; i.imageSpeed=0; i.timer=0;
            }
        }
        if ((mainPickup || plusPickup) && i.state == 1) {
            i.y -= 1;
            const unsigned alpha = (i.colour >> 24) & 0xffU;
            const unsigned nextAlpha = alpha > 18 ? alpha - 18 : 0;
            i.colour = (i.colour & 0x00ffffffU) | (nextAlpha << 24);
            if (nextAlpha < 26) {
                if (i.object == "obj_chocolate") ++chocolates_;
                else if (i.object == "obj_lollipop") haveDash_ = true;
                else if (i.object == "obj_pocket") haveBlink_ = true;
                else if (i.object == "obj_sock") haveSlide_ = true;
                else ++plusItems_;
                i.active = false;
            }
            continue;
        }
        if (obj->behavior == "none") continue;
        const float distSquared = distanceSquared(i);
        const float rangeSquared = i.range*i.range;
        if (obj->behavior == "eslide" || obj->behavior == "eslide_boss") {
            if (distSquared < rangeSquared && i.state == 0) {
                i.direction = obj->behavior == "eslide_boss" ? 1.0F : -sign(player_.hsp);
                i.state = 1;
                i.imageIndex = 1;
            }
            if (i.state == 1) { i.imageIndex=1; i.imageSpeed=0; }
            if (i.state == 1) i.x += i.direction * i.speed;
            if (i.state == 1 && i.object=="obj_gas") i.imageAngle-=10;
            if (i.object=="obj_car") i.scaleX=i.x>player_.x ? -std::abs(i.scaleX) : std::abs(i.scaleX);
            if (i.state == 1 && i.object=="obj_chair") spawnScratch_.push_back(makeInstance("obj_trailchair",i.x,i.y));
        } else if (obj->behavior == "efall") {
            if (i.state == 0 && distSquared < rangeSquared) { i.state = 1; i.imageIndex = 1; }
            if (i.state == 1) { i.imageIndex=1; i.imageSpeed=0; }
            if (i.state == 1) {
                const float oldY = i.y;
                i.y += i.verticalSpeed;
                const Rectangle bounds = instanceBounds(i);
                for (const std::size_t wallIndex:collisionIndices_) {
                    if(wallIndex==instanceIndex||wallIndex>=room_->instances.size())continue;
                    const auto& wall=room_->instances[wallIndex];
                    if(wall.active&&CheckCollisionRecs(bounds,instanceBounds(wall))){i.active=false;break;}
                }
                for(std::size_t wallIndex=indexedInstanceCount_;i.active&&wallIndex<room_->instances.size();++wallIndex) {
                    const auto& wall=room_->instances[wallIndex];
                    if(wallIndex!=instanceIndex&&wall.active&&inherits(wall.object,"obj_colision")&&
                       CheckCollisionRecs(bounds,instanceBounds(wall))){i.active=false;break;}
                }
                if (!i.active) i.y = oldY;
            }
        } else if (obj->behavior == "efollow") {
            if (distSquared < rangeSquared && i.state == 0) i.state = 1;
            if (i.state == 1 && ++i.timer > 60) { i.state = 2; i.timer = 0; i.imageIndex = 1; }
            if (i.state == 2) {
                i.imageIndex=1;i.imageSpeed=0;
                if (distSquared > rangeSquared) { i.state = 0; i.imageIndex = 0; }
                else {
                    const float angle = std::atan2(player_.y - i.y, player_.x - i.x);
                    i.x += std::cos(angle) * 2.0F; i.y += std::sin(angle) * 2.0F;
                }
            }
            if (i.state==1 || i.state==2) i.imageAngle=std::sin(stepCounter_*0.12F)*9.0F;
        } else if (obj->behavior == "ethrow" || obj->behavior == "ethrow_range") {
            if (obj->behavior == "ethrow_range" && i.state == -1 && distSquared < rangeSquared) i.state = 0;
            if (i.state == 0) {
                if (obj->behavior == "ethrow_range") { i.imageIndex=1;i.imageSpeed=0; }
                const float angle = std::atan2(player_.y - i.y, player_.x - i.x);
                i.direction = angle;
                if (i.object=="obj_fireball") i.state=1;
                else {
                    ++i.timer;
                    if (i.timer < 40) i.y -= 1;
                    else if (i.timer > 50) { i.state = 1; i.timer = 0; }
                }
            }
            if (i.state == 1) {
                if (i.object=="obj_fireball" && i.scaleX<1.0F) i.scaleX=i.scaleY=std::min(1.0F,i.scaleX+0.1F);
                const float dx=std::cos(i.direction)*i.speed, dy=std::sin(i.direction)*i.speed;
                const bool bouncer=i.object=="obj_ball" || i.object=="obj_tire";
                bool blocked=false;
                if (bouncer) {
                    InstanceDef probe=i; probe.x+=dx; probe.y+=dy;
                    for (const auto& wall:room_->instances) {
                        const auto* wallObject=object(wall.object);
                        if (&wall!=&i && wall.active && wallObject && wallObject->solid &&
                            instanceCollision(instanceBounds(probe),wall)) { blocked=true; break; }
                    }
                }
                if (blocked) i.direction += PI;
                else { i.x += dx; i.y += dy; }
                i.imageAngle = bouncer ? i.direction*RAD2DEG : i.imageAngle+10;
                if (i.object=="obj_ball" && i.spriteOverride!="spr_football")
                    spawnScratch_.push_back(makeInstance("obj_trailball",i.x,i.y));
            }
        } else if (obj->behavior == "espawner") {
            ++i.timer;
            if (i.verticalSpeed>0) {
                --i.verticalSpeed;
                i.scaleX-=0.1F;
                if (i.object=="obj_candle") i.scaleY-=0.1F;
            } else {
                i.scaleX=std::min(1.0F,i.scaleX+0.1F);
                if (i.object=="obj_candle") i.scaleY=std::min(1.0F,i.scaleY+0.1F);
            }
            std::string spawnName=obj->spawnObject;
            if (i.object=="obj_dish_washer") spawnName=GetRandomValue(0,1) ? "obj_mug" : "obj_dish";
            else if (i.object=="obj_toolbox") spawnName="obj_tools";
            if (distSquared < rangeSquared) {i.imageIndex=1;i.imageSpeed=0;}
            else {i.imageIndex=0;i.imageSpeed=0;}
            if (distSquared < rangeSquared && i.timer > obj->spawnRate && !spawnName.empty()) {
                spawnScratch_.push_back(makeInstance(spawnName, i.x, i.y));
                i.timer = 0;
                i.verticalSpeed=5;
            }
            if (i.object=="obj_candle") {
                const float dx=std::cos(i.direction)*2.0F, dy=std::sin(i.direction)*2.0F;
                InstanceDef probe=i; probe.x+=dx; probe.y+=dy;
                bool blocked=false;
                for (const auto& wall:room_->instances) {
                    const auto* wallObject=object(wall.object);
                    if (&wall!=&i && wall.active && wallObject && wallObject->solid &&
                        instanceCollision(instanceBounds(probe),wall)) { blocked=true; break; }
                }
                if (blocked) i.direction+=PI; else { i.x+=dx; i.y+=dy; }
            }
        } else if (obj->behavior == "trash_ball") {
            i.x += i.speed; i.imageAngle -= 5;
            const float oldY = i.y; i.y += 2;
            const Rectangle bounds=instanceBounds(i);
            bool blocked=false;
            for(const std::size_t wallIndex:collisionIndices_) {
                if(wallIndex==instanceIndex||wallIndex>=room_->instances.size())continue;
                const auto& wall=room_->instances[wallIndex];
                if(wall.active&&CheckCollisionRecs(bounds,instanceBounds(wall))){blocked=true;break;}
            }
            for(std::size_t wallIndex=indexedInstanceCount_;!blocked&&wallIndex<room_->instances.size();++wallIndex) {
                const auto& wall=room_->instances[wallIndex];
                if(wallIndex!=instanceIndex&&wall.active&&inherits(wall.object,"obj_colision")&&
                   CheckCollisionRecs(bounds,instanceBounds(wall)))blocked=true;
            }
            if(blocked)i.y=oldY;
        } else if (obj->behavior == "eye_left") {
            if (i.timer++ == 0) i.direction = std::atan2(player_.y-i.y, player_.x-i.x);
            i.x += std::cos(i.direction)*i.speed; i.y += std::sin(i.direction)*i.speed;
        } else if (obj->behavior == "eye_right") {
            if (++i.timer == 16) i.direction = std::atan2(player_.y-i.y, player_.x-i.x);
            if (i.timer >= 16) { i.x += std::cos(i.direction)*i.speed; i.y += std::sin(i.direction)*i.speed; }
        } else if (obj->behavior == "boss_car") {
            if (i.scaleX<1.1F) { i.scaleX+=0.05F; i.scaleY+=0.05F; }
            if (++i.timer <= 30) { i.x += 2.5F; i.y -= 2.5F; }
            else {
                if (i.timer < 50) i.direction = std::atan2(player_.y-i.y, player_.x-i.x);
                else { i.x += std::cos(i.direction)*i.speed; i.y += std::sin(i.direction)*i.speed; }
            }
        }
        if (i.x < -100 || i.x > room_->width + 100 || i.y < -32 || i.y > room_->height + 32) i.active = false;
    }
    room_->instances.insert(room_->instances.end(), spawnScratch_.begin(), spawnScratch_.end());
}

InstanceDef Game::makeInstance(const std::string& objectName, float x, float y) const {
    InstanceDef i{};
    i.object = objectName; i.name = "runtime"; i.x = x; i.y = y;
    i.previousX=x; i.previousY=y; i.active = true;
    if (const auto* obj = object(objectName)) {
        i.state=obj->state; i.timer=obj->timer; i.type=obj->type; i.range=obj->range; i.speed=obj->speed;
        i.verticalSpeed=obj->verticalSpeed; i.direction=obj->direction;
        const auto* spr = sprite(obj->sprite);
        i.imageSpeed = obj->animationSpeed >= 0 ? obj->animationSpeed : (spr ? spr->fps/45.0F : 0.0F);
    }
    if (objectName == "obj_ball") {
        static const char* variants[] = {"spr_basket", "spr_ball", "spr_football"};
        i.spriteOverride = variants[GetRandomValue(0, 2)];
    }
    if (objectName == "obj_littlepaint")
        i.spriteOverride="spr_lilpaint"+std::string(TextFormat("%02d",GetRandomValue(1,13)));
    if (objectName == "obj_paint" || objectName == "obj_paint2")
        i.spriteOverride="spr_paint"+std::string(TextFormat("%02d",GetRandomValue(1,21)));
    if (objectName == "obj_fireball") i.scaleX=i.scaleY=0.5F;
    if (objectName == "obj_bosscar") { i.scaleX=i.scaleY=0.25F; i.imageIndex=1; }
    return i;
}

void Game::updateBoss() {
    if (room_->name != "rm_boss" || player_.dead) return;
    auto boss = std::find_if(room_->instances.begin(), room_->instances.end(),
        [](const InstanceDef& i) { return i.active && i.object == "obj_boss"; });
    if (boss == room_->instances.end()) return;
    const auto setBossSprite=[&](const char* name) {
        if (boss->spriteOverride!=name) {
            boss->spriteOverride=name;
            boss->imageIndex=0.0F;
        }
    };
    if (bossLife_ < 60) {
        setBossSprite("spr_boss_death");
        if (++bossDeathTimer_ > 130 && !touchesObject("obj_next"))
            room_->instances.push_back(makeInstance("obj_next", player_.x, player_.y));
        return;
    }
    ++bossTimer_;
    if (bossState_ == 1) {
        setBossSprite("spr_boss_idle");
        ++bossAttackTimer_;
        if (bossAttackTimer_ == 1 || bossAttackTimer_ == 15 || bossAttackTimer_ == 30)
            room_->instances.push_back(makeInstance("obj_trash_ball", 96, 165));
    } else if (bossState_ == 2) {
        if (bossEyeTimer_==0) setBossSprite("spr_boss_eyeatk");
        ++bossEyeTimer_;
        if (boss->spriteOverride=="spr_boss_eyeatk" && boss->imageIndex>15.0F) {
            room_->instances.push_back(makeInstance("obj_eyel", 295, 50));
            room_->instances.push_back(makeInstance("obj_eyer", 250, 50));
            setBossSprite("spr_boss_eyegrown");
        }
        if (boss->spriteOverride=="spr_boss_eyegrown" && boss->imageIndex>8.0F)
            setBossSprite("spr_boss_idle");
    } else if (bossState_ == 3) {
        setBossSprite("spr_boss_idle");
        ++bossAttackTimer_;
        if (bossAttackTimer_ == 1 || bossAttackTimer_ == 60)
            room_->instances.push_back(makeInstance("obj_bosscar", 90, 160));
    } else {
        setBossSprite("spr_boss_idle");
    }
    if (bossTimer_ > bossTiming_) {
        if (bossState_ != 0) bossLife_ -= 26.4910248987F;
        bossState_ = (bossState_ + 1) % 4;
        bossTiming_ = bossState_ == 0 ? 30 : (bossState_ == 1 ? 60 : 120);
        bossTimer_ = bossAttackTimer_ = bossEyeTimer_ = 0;
        setBossSprite("spr_boss_idle");
        if (bossState_ == 2 || bossState_ == 3) playSound("sfx_bossroar", 0.5F);
        else if (bossState_ == 1) playSound("sfx_boss2", 0.5F);
    }
}

void Game::update() {
    ++stepCounter_;
    ++roomStepCounter_;
#ifdef __3DS__
    // Music decode is asynchronous on 3DS. Retry the zero-cost handoff each
    // frame so entering a room never waits for the decoder thread.
    setRoomMusic();
    preloadNextRoomMusic();
#endif
#ifndef __EMSCRIPTEN__
    if (musicLoaded_) UpdateMusicStream(music_);
#endif
    if (!paused_) {
        for(const std::size_t index:animationIndices_) {
            if(index>=room_->instances.size())continue;
            auto& instance=room_->instances[index];
            if(instance.active)instance.imageIndex+=instance.imageSpeed;
        }
        // Dynamically spawned effects live beyond the last rebuilt static index.
        for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index) {
            auto& instance=room_->instances[index];
            if(instance.active&&instance.imageSpeed!=0.0F)instance.imageIndex+=instance.imageSpeed;
        }
    }
    if (roomFadeFrames_ > 0) --roomFadeFrames_;
    if (!paused_ && titleFrames_ > 0) --titleFrames_;
    if (input_.collision) showCollision_ = !showCollision_;
    const std::string& scene = room_->name;
    const auto go = [&](const std::string& name) {
        auto it = std::find(data_.roomOrder.begin(), data_.roomOrder.end(), name);
        if (it != data_.roomOrder.end()) loadRoom(static_cast<std::size_t>(it - data_.roomOrder.begin()));
    };
    const bool upPressed=input_.up;
    const bool downPressed=input_.down;
    bool confirm=input_.confirm;
    const bool clicked=input_.click;
    const Vector2 mouse=input_.mouse;
    if (scene == "rm_splash") {
        if (++sceneTimer_ >= 98 || confirm) go("rm_language");
    } else if (scene == "rm_language") {
        if (clicked) { menuSelection_=mouse.y<109 ? 0 : 1; confirm=true; }
        if (upPressed || downPressed) menuSelection_ = 1 - menuSelection_;
        if (confirm) { language_ = menuSelection_; menuSelection_ = 0; go("rm_menu"); }
    } else if (scene == "rm_menu") {
        const int menuCount=
#ifdef __3DS__
            3;
#else
            4;
#endif
        if (clicked && mouse.x>=
#ifdef __3DS__
            96 && mouse.x<=224
#else
            102 && mouse.x<=282
#endif
        ) for (int i=0;i<menuCount;++i)
            if (std::abs(mouse.y-(
#ifdef __3DS__
                96+i*24
#else
                128+i*24
#endif
            ))<=12) { menuSelection_=i; confirm=true; }
        if (upPressed) menuSelection_ = (menuSelection_ + menuCount-1) % menuCount;
        if (downPressed) menuSelection_ = (menuSelection_ + 1) % menuCount;
        if (confirm) {
            if (menuSelection_ == 0) {
                chocolates_ = deaths_ = elapsedFrames_ = plusItems_ = 0; plusMode_ = false; skin_ = -1;
                haveSlide_ = haveDash_ = haveBlink_ = false; go("rm_0");
            }
            else if (menuSelection_ == 1 && gameCompleted_) {
                chocolates_ = deaths_ = elapsedFrames_ = plusItems_ = 0; plusMode_ = true; skin_ = GetRandomValue(0, 1);
                haveSlide_ = haveDash_ = haveBlink_ = true; go("rm_0");
            } else if (menuSelection_ == 2) go("rm_credits");
#ifndef __3DS__
            else if (menuSelection_ == 3) running_ = false;
#endif
        }
    } else if (scene == "rm_credits") {
        if (input_.escape || confirm || clicked) { menuSelection_ = 0; go("rm_menu"); }
    } else if (scene == "rm_boss_scene") {
        auto actor = std::find_if(room_->instances.begin(), room_->instances.end(),
            [](const InstanceDef& i){ return i.active && i.object == "obj_player_cutscene"; });
        if (actor != room_->instances.end()) {
            actor->x += 4;
            InstanceDef probe=*actor;probe.y+=2;
            const Rectangle candidate=instanceBounds(probe);
            const bool floor=std::any_of(room_->instances.begin(),room_->instances.end(),[&](const InstanceDef& wall) {
                return &wall!=&*actor && wall.active && inherits(wall.object,"obj_colision") &&
                       instanceCollision(candidate,wall);
            });
            if(!floor)actor->y+=2;
            if (actor->x > 390) go("rm_boss");
        }
    } else if (scene == "rm_end") {
        if (sceneTimer_ == 0) {
            gameCompleted_ = true;
            if (bestFrames_ == 0 || elapsedFrames_ < bestFrames_) bestFrames_ = elapsedFrames_;
            saveGame();
        }
        ++sceneTimer_;
        if (endingAnim_ > -90) endingAnim_ -= 0.17F;
        if (trueEnding_ && sceneTimer_ > 600) {
            trueEnding_ = false;
            sceneTimer_ = 0;
            roomFadeMax_ = roomFadeFrames_ = 100;
        } else if (!trueEnding_ && sceneTimer_ > 180) {
            menuSelection_ = 0; go("rm_menu");
        }
    } else if (hasPlayer_) {
        if (input_.escape) {
            const bool wasPaused=paused_;
            paused_ = !paused_;
            if (!wasPaused) confirm=false;
            pauseSelection_ = 0;
            if (musicLoaded_) { if (paused_) PauseMusicStream(music_); else ResumeMusicStream(music_); }
        }
        if (paused_) {
            const int pauseCount=
#if defined(__3DS__) || defined(__PSP__)
                4;
#else
                5;
#endif
            if (clicked && mouse.x>=
#ifdef __3DS__
                92 && mouse.x<=228
#else
                102 && mouse.x<=282
#endif
            ) for (int i=0;i<pauseCount;++i) {
#if defined(__3DS__) || defined(__PSP__)
                static constexpr float touchRows[]={72,96,120,168};
                const float row=touchRows[i];
#else
                const float row=72+i*24;
#endif
                if (std::abs(mouse.y-row)<=13) { pauseSelection_=i; confirm=true; }
            }
            if (upPressed) pauseSelection_ = (pauseSelection_ + pauseCount-1) % pauseCount;
            if (downPressed) pauseSelection_ = (pauseSelection_ + 1) % pauseCount;
            if (confirm) {
                if (pauseSelection_ == 0) paused_ = false;
                else if (pauseSelection_ == 1) {
                    chocolates_ = deaths_ = elapsedFrames_ = 0;
                    if (!plusMode_) haveSlide_ = haveDash_ = haveBlink_ = false;
                    paused_ = false; go("rm_0");
                }
#if defined(__3DS__) || defined(__PSP__)
                else if (pauseSelection_ == 2) { paused_ = false; menuSelection_ = 0; go("rm_menu"); }
                else if (pauseSelection_ == 3) {
#else
                else if (pauseSelection_ == 2) ToggleFullscreen();
                else if (pauseSelection_ == 3) { paused_ = false; menuSelection_ = 0; go("rm_menu"); }
                else if (pauseSelection_ == 4) {
#endif
                    muted_ = !muted_;
                    SetMasterVolume(muted_ ? 0.0F : 1.0F);
                }
            }
        }
    }
    if (input_.restart && hasPlayer_) loadRoom(roomIndex_, true);
    if (transitioning_ && !paused_) {
        if (++transitionTimer_ > 90) loadRoom(roomIndex_ + 1);
    } else if (hasPlayer_ && !paused_) {
        if (plusMode_) ++elapsedFrames_;
        updateBoss(); updateInstances(); updatePlayer();
    }
    if (hasPlayer_) {
        float viewX = camera_.target.x - kViewWidth/2.0F;
        float viewY = camera_.target.y - kViewHeight/2.0F;
        if (player_.x < viewX + 190) viewX = player_.x - 190;
        else if (player_.x > viewX + kViewWidth - 190) viewX = player_.x - (kViewWidth - 190);
        if (player_.y < viewY + 106) viewY = player_.y - 106;
        else if (player_.y > viewY + kViewHeight - 106) viewY = player_.y - (kViewHeight - 106);
        viewX = std::clamp(viewX, 0.0F, std::max(0.0F, room_->width-(float)kViewWidth));
        viewY = std::clamp(viewY, 0.0F, std::max(0.0F, room_->height-(float)kViewHeight));
        camera_.target = {viewX+kViewWidth/2.0F, viewY+kViewHeight/2.0F};
    }
#ifndef __PSP__
    rebuildDrawList();
#endif
}

Color Game::gmColour(std::uint32_t value) const {
    const unsigned char alpha = value <= 0x00FFFFFFU ? 255 : static_cast<unsigned char>((value >> 24) & 0xFF);
    return {static_cast<unsigned char>(value & 0xFF), static_cast<unsigned char>((value >> 8) & 0xFF),
            static_cast<unsigned char>((value >> 16) & 0xFF), alpha};
}

void Game::drawSprite(const std::string& name, float frame, float x, float y, float scaleX,
                      float scaleY, float rotation, std::uint32_t tint, const Rectangle* source) {
    const auto* definition = sprite(name);
    auto& loaded = texture(name).frames;
    if (!definition || loaded.empty()) return;
    const auto index = static_cast<std::size_t>(std::max(0.0F, std::floor(frame))) % loaded.size();
    const auto& image = loaded[index];
    Rectangle src = source ? *source : Rectangle{0, 0, static_cast<float>(image.width), static_cast<float>(image.height)};
    if (scaleX < 0) src.width = -src.width;
    if (scaleY < 0) src.height = -src.height;
    Rectangle dst{x, y, std::abs(src.width * scaleX), std::abs(src.height * scaleY)};
    // GameMaker mirrors around the instance origin. DrawTexturePro flips only the
    // sampled pixels, so its destination origin must be moved to the opposite
    // edge as well or negative-scale sprites appear on the wrong side.
    Vector2 origin{
        (scaleX < 0 ? std::abs(src.width)-definition->originX : definition->originX) * std::abs(scaleX),
        (scaleY < 0 ? std::abs(src.height)-definition->originY : definition->originY) * std::abs(scaleY)};
    DrawTexturePro(image, src, dst, origin, rotation, gmColour(tint));
}

void Game::drawWorld() {
#if defined(__3DS__) || defined(__PSP__)
    // Citro2D has a finite per-frame geometry pool. Large rooms contain
    // thousands of legacy tiles, so submitting the whole room eventually
    // exhausted that pool and everything later in the draw order vanished.
    // This changes only submission, never simulation or room loading.
    const float viewLeft=camera_.target.x-kViewWidth/2.0F;
    const float viewTop=camera_.target.y-kViewHeight/2.0F;
    const Rectangle visibleView{viewLeft,viewTop,kViewWidth,kViewHeight};
#endif
#ifdef __3DS__
    // Use a deliberately wide depth range. Backgrounds sit behind the screen,
    // while active enemies and UI project towards the player. The menu title
    // and room titles inherit stereoUi below, matching the UI plane exactly.
    constexpr float stereoBackground=-0.25F;
    constexpr float stereoDecoration=0.15F;
    constexpr float stereoInactiveEnemy=0.45F;
    constexpr float stereoStructureAndPlayer=0.75F;
    constexpr float stereoActiveEnemy=1.10F;
    constexpr float stereoUi=1.45F;
#endif
    for (const auto& item : drawScratch_) {
        if (item.kind==DrawKind::Player) {
#ifdef __3DS__
            Set3DStereoLayer(stereoStructureAndPlayer);
#endif
            drawSprite(player_.sprite,player_.animation,
                mix(player_.previousX,player_.x,renderAlpha_),mix(player_.previousY,player_.y,renderAlpha_),
                player_.facing,1,0,0xFFFFFFFF);
            if (room_->name!="rm_boss") {
                const float viewX=camera_.target.x-kViewWidth/2.0F;
                const float viewY=camera_.target.y-kViewHeight/2.0F;
                drawSprite("spr_effect",0,viewX,viewY,1,1,0,0xFFFFFFFF);
            }
        } else if (item.kind==DrawKind::Background) {
#ifdef __3DS__
            Set3DStereoLayer(stereoBackground);
#endif
            const auto& bg=room_->backgrounds[item.index];
            if (bg.sprite.empty()) { DrawRectangle(0, 0, room_->width, room_->height, gmColour(bg.colour)); continue; }
            const auto* spr = sprite(bg.sprite);
            if (!spr) continue;
            if (bg.stretch) {
                auto& frames = texture(bg.sprite).frames;
                if (!frames.empty()) DrawTexturePro(frames[0], {0,0,(float)frames[0].width,(float)frames[0].height},
                    {0,0,(float)room_->width,(float)room_->height}, {0,0}, 0, gmColour(bg.colour));
            } else {
                const float renderTick=std::max(0.0F,static_cast<float>(roomStepCounter_)-1.0F+renderAlpha_);
                float startX=bg.x+bg.hspeed*renderTick, startY=bg.y+bg.vspeed*renderTick;
                if (bg.tileX) startX=std::fmod(startX,(float)std::max(1,spr->width))-spr->width;
                if (bg.tileY) startY=std::fmod(startY,(float)std::max(1,spr->height))-spr->height;
                int maxX = bg.tileX ? room_->width+spr->width : static_cast<int>(startX + spr->width);
                int maxY = bg.tileY ? room_->height+spr->height : static_cast<int>(startY + spr->height);
#if defined(__3DS__) || defined(__PSP__)
                if(bg.tileX) {
                    startX+=std::floor((visibleView.x-startX)/std::max(1,spr->width))*std::max(1,spr->width);
                    maxX=static_cast<int>(visibleView.x+visibleView.width+spr->width);
                }
                if(bg.tileY) {
                    startY+=std::floor((visibleView.y-startY)/std::max(1,spr->height))*std::max(1,spr->height);
                    maxY=static_cast<int>(visibleView.y+visibleView.height+spr->height);
                }
#endif
                for (int y = static_cast<int>(startY); y < maxY; y += std::max(1, spr->height))
                    for (int x = static_cast<int>(startX); x < maxX; x += std::max(1, spr->width))
                        drawSprite(bg.sprite, 0, (float)x, (float)y, 1, 1, 0, bg.colour);
            }
        } else if (item.kind==DrawKind::Graphic) {
#ifdef __3DS__
            // Legacy graphic tiles form the floor, walls and ceiling.
            Set3DStereoLayer(stereoStructureAndPlayer);
#endif
#ifdef __PSP__
            // The guarded draw list is reused across camera movement. Reject
            // its off-screen margin before doing any texture work, then submit
            // the precomputed tile data without per-frame string hashes or
            // source/destination reconstruction.
            if(!CheckCollisionRecs(graphicBounds_[item.index],visibleView))continue;
            const auto& prepared=preparedGraphics_[item.index];
            if(prepared.image.id)
                DrawTexturePro(prepared.image,prepared.source,prepared.destination,{0,0},prepared.rotation,prepared.tint);
#else
            const auto& g=room_->graphics[item.index];
            // Compatibility asset layers are GameMaker's legacy tiles. Their
            // x/y is the top-left of the cropped tile, independent of the
            // source sprite's origin (unlike draw_sprite_ext()).
            auto& frames = texture(g.sprite).frames;
            if (!frames.empty()) {
                const auto& image = frames[0];
                const float rawWidth=g.width != 0 ? g.width : static_cast<float>(image.width);
                const float rawHeight=g.height != 0 ? g.height : static_cast<float>(image.height);
                const bool flipX=(rawWidth<0)!=(g.scaleX<0);
                const bool flipY=(rawHeight<0)!=(g.scaleY<0);
                const float sourceWidth=std::abs(rawWidth),sourceHeight=std::abs(rawHeight);
                // A tiny inward source inset prevents neighbouring cells in a
                // tileset from leaking through at fractional camera positions.
                // Destination dimensions remain exact, so no tile is resized.
                constexpr float inset=0.01F;
                Rectangle src{g.u0+inset,g.v0+inset,sourceWidth-inset*2,sourceHeight-inset*2};
                if (flipX) src.width=-src.width;
                if (flipY) src.height=-src.height;
                Rectangle dst{g.x,g.y,sourceWidth*std::abs(g.scaleX),sourceHeight*std::abs(g.scaleY)};
                DrawTexturePro(image, src, dst, {0,0}, g.rotation, gmColour(g.colour));
            }
#endif
        } else {
            const auto& instance=room_->instances[item.index];
            const auto* obj = object(instance.object);
#ifdef __3DS__
            float instanceStereo=stereoDecoration;
            if(obj) {
                const bool enemy=inherits(instance.object,"obj_evil") || obj->behavior!="none" ||
                                 instance.object=="obj_boss" || instance.object=="obj_boss_atk";
                if(enemy) {
                    const bool activated=instance.name=="runtime" || instance.state>obj->state ||
                                         instance.object=="obj_boss" || instance.object=="obj_boss_atk";
                    instanceStereo=activated?stereoActiveEnemy:stereoInactiveEnemy;
                } else if(inherits(instance.object,"obj_colision") || instance.object=="obj_staircase" ||
                          instance.object=="obj_spike" || instance.object=="obj_spikeinv") {
                    instanceStereo=stereoStructureAndPlayer;
                }
            }
            Set3DStereoLayer(instanceStereo);
#endif
            float renderX=mix(instance.previousX,instance.x,renderAlpha_);
            float renderY=mix(instance.previousY,instance.y,renderAlpha_);
#ifdef __3DS__
            if(room_->name=="rm_credits" && instance.object=="obj_guris") {
                // The credits text moved to the lower display, leaving the
                // original portrait as the centred upper-screen focal point.
                renderX=kViewWidth/2.0F;
                renderY=kViewHeight/2.0F;
            }
#endif
            std::uint32_t tint = instance.colour;
            if (instance.object == "obj_fadein") {
                // Original Step event subtracts 0.1 image_alpha per frame and
                // destroys the instance at zero.
                if (roomStepCounter_ >= 10) continue;
                const auto alpha=static_cast<std::uint32_t>(
                    std::clamp(1.0F-roomStepCounter_*0.1F,0.0F,1.0F)*255.0F);
                tint=(tint&0x00FFFFFFU)|(alpha<<24);
            }
            if (instance.object == "obj_splash") {
                const float alpha = sceneTimer_ < 41 ? sceneTimer_/40.0F : std::clamp(1.0F-(sceneTimer_-57)/40.0F,0.0F,1.0F);
                tint = (tint & 0x00FFFFFFU) | (static_cast<std::uint32_t>(alpha*255) << 24);
            }
            if (instance.object=="obj_title" || instance.object=="obj_platform") continue;
            if (obj && obj->visible) {
                const std::string& spriteName=instance.spriteOverride.empty() ? obj->sprite : instance.spriteOverride;
#ifdef __3DS__
                if(instance.object=="obj_spike" && instance.scaleY<0.0F) {
                    // Tex3DS may rotate small atlas entries. Express the exact
                    // vertical mirror as a horizontal mirror plus 180 degrees;
                    // this uses the reliable horizontal UV path and preserves
                    // the original GameMaker pivot and collision placement.
                    drawSprite(spriteName,instance.imageIndex,renderX,renderY,
                        -instance.scaleX,-instance.scaleY,instance.imageAngle+180.0F,tint);
                } else
#endif
                drawSprite(spriteName,instance.imageIndex,renderX,renderY,
                    instance.scaleX,instance.scaleY,instance.imageAngle,tint);
            }
        }
    }
#ifdef __3DS__
    // Room titles, prompts and debug overlays occupy the nearest plane.
    Set3DStereoLayer(stereoUi);
#endif
    if (titleFrames_ > 0) {
        static const std::unordered_map<std::string,std::string> en{{"rm_0","spr_en_bedroom"},{"rm_1","spr_en_stairs"},
            {"rm_2","spr_en_kitchen"},{"rm_3","spr_en_living"},{"rm_4","spr_en_corridor"},{"rm_5","spr_en_garage"}};
        static const std::unordered_map<std::string,std::string> pt{{"rm_0","spr_title_bedroom"},{"rm_1","spr_title_stairs"},
            {"rm_2","spr_title_kitchen"},{"rm_3","spr_title_living"},{"rm_4","spr_title_corridor"},{"rm_5","spr_title_garage"}};
        const auto& map = language_ ? pt : en;
        if (auto found=map.find(room_->name); found!=map.end()) {
            const float tx=room_->name=="rm_2"?96.0F:192.0F, ty=room_->name=="rm_2"?400.0F:109.0F;
            const std::uint32_t alpha=static_cast<std::uint32_t>(std::clamp(titleFrames_/50.0F,0.0F,1.0F)*255);
            drawSprite(found->second,0,tx,ty,1,1,0,0x00FFFFFFU|(alpha<<24));
        }
    }
    if (showCollision_) {
        for (const auto& instance : room_->instances) if (instance.active && inherits(instance.object, "obj_colision"))
            DrawRectangleLinesEx(instanceBounds(instance), 1, RED);
        DrawRectangleLinesEx(playerBounds(player_.x, player_.y), 1, LIME);
    }
}

void Game::drawLighting() {
    if (!roomLighting_) return;
    BeginTextureMode(lightTarget_);
    ClearBackground(gmColour(5482610));
    beginGmSubtract();
    const auto hole = [&](float worldX, float worldY, float diameter, Color centre) {
        const Vector2 screen = GetWorldToScreen2D({worldX, worldY}, camera_);
        const float radius=diameter/2.0F;
        if(screen.x+radius<0 || screen.y+radius<0 ||
           screen.x-radius>=kViewWidth || screen.y-radius>=kViewHeight)return;
        DrawCircleGradient(static_cast<int>(screen.x), static_cast<int>(screen.y), radius, centre, BLACK);
    };
    const auto drawLight=[&](const InstanceDef& i) {
        if (!i.active) return;
        const float x=mix(i.previousX,i.x,renderAlpha_), y=mix(i.previousY,i.y,renderAlpha_);
        if (i.object == "obj_lighting") hole(x,y,300,WHITE);
        else if (i.object == "obj_lighthouse") hole(x,y,150,gmColour(255));
        else if (i.object == "obj_bestlight") hole(x,y,150,gmColour(4235519));
        else if (i.object == "obj_candle") hole(x,y,200,gmColour(6053119));
        else if (i.object == "obj_fireball") hole(x,y,100,gmColour(4235519));
        else if (i.object == "obj_trash_ball") hole(x,y,100,WHITE);
        else if (i.object == "obj_chocolate" || i.object == "obj_lollipop" ||
                 i.object == "obj_pocket" || i.object == "obj_sock" ||
                 i.object == "obj_bible" || i.object == "obj_cruz" ||
                 i.object == "obj_oil" || i.object == "obj_vela" || i.object == "obj_water")
            hole(x,y,48,WHITE);
        else if (i.object == "obj_boss" && bossState_ == 1) hole(x,y,200,WHITE);
        else if (i.object == "obj_wallamp" || i.object == "obj_wallamp2") hole(x,y,128,gmColour(4235519));
        else if (i.object == "obj_window" || i.object == "obj_window2") hole(x,y,64,gmColour(16751475));
    };
    for(const std::size_t index:lightIndices_)
        if(index<room_->instances.size())drawLight(room_->instances[index]);
    for(std::size_t index=indexedInstanceCount_;index<room_->instances.size();++index)
        drawLight(room_->instances[index]);
    if (hasPlayer_) hole(mix(player_.previousX,player_.x,renderAlpha_),mix(player_.previousY,player_.y,renderAlpha_),100,WHITE);
    EndBlendMode();
    EndTextureMode();
}

void Game::draw() {
    const Vector2 logicalCameraTarget=camera_.target;
#ifdef __3DS__
    const char* lowerSignMessage=nullptr;
#endif
    camera_.target={mix(previousCameraTarget_.x,camera_.target.x,renderAlpha_),
                    mix(previousCameraTarget_.y,camera_.target.y,renderAlpha_)};
#ifdef __PSP__
    // Membership and depth order rarely change every rendered frame. Re-query
    // when the camera enters another 32-pixel cell or at a bounded cadence for
    // newly activated/spawned objects; positions still interpolate every frame.
    const int drawCellX=static_cast<int>(std::floor(camera_.target.x/32.0F));
    const int drawCellY=static_cast<int>(std::floor(camera_.target.y/32.0F));
    if(!drawListValid_ || drawCellX!=drawListCameraCellX_ || drawCellY!=drawListCameraCellY_ ||
       stepCounter_-drawListStep_>=4) {
        rebuildDrawList();
        drawListStep_=stepCounter_;
        drawListCameraCellX_=drawCellX;drawListCameraCellY_=drawCellY;
        drawListValid_=true;
    }
#endif
    BeginTextureMode(target_);
    ClearBackground(BLACK);
    BeginMode2D(camera_);
    drawWorld();
    EndMode2D();
    EndTextureMode();

    drawLighting();

    BeginTextureMode(target_);
    if (roomLighting_) {
#ifdef __3DS__
        // Keep the full-screen mask near the structural plane so its holes
        // remain aligned with the player and the room architecture.
        Set3DStereoLayer(0.75F);
#endif
        beginGmSubtract();
        DrawTextureRec(lightTarget_.texture,
            {0, 0, (float)lightTarget_.texture.width, -(float)lightTarget_.texture.height}, {0, 0}, WHITE);
        EndBlendMode();
    }
#ifdef __3DS__
    Set3DStereoLayer(1.45F);
#endif
    if (hasPlayer_ && !paused_ && !player_.dead) {
#ifndef __3DS__
        if (!plusMode_) {
            drawSprite("spr_chocolat", 0, 15, 15, 1, 1, 0, 0xFFFFFFFF);
            text(TextFormat("%d", chocolates_), 25, 11, 12);
        }
        drawSprite("spr_cavera", 0, 330, 205, 1, 1, 0, 0xFFFFFFFF);
        text(TextFormat("%d", deaths_), 340, 200, 12);
#endif
    }
    if (plusMode_ && (hasPlayer_ || room_->name == "rm_end") && !paused_) {
        const float pos = room_->name == "rm_end" ? std::min(148, sceneTimer_*2) : 0;
        drawSprite("spr_stopwatch", elapsedFrames_*0.25F, 13+pos, 13+pos/1.2F, 1, 1, 0, 0xFFFFFFFF);
        const int last=static_cast<int>(std::round((elapsedFrames_%60)*1.0101010101F));
        text(TextFormat("%d:%02d:%02d", elapsedFrames_/3600, (elapsedFrames_/60)%60, last), 25+pos, 10+pos/1.2F, 8);
    }
    if (activeSign_ > 0) {
        static const char* en[] = {"", "Hello human..\nLet's play a game?\ntry to run away from home\nthat you humans\ninvaded and if you can\nI let you live...\nGood luck.",
            "I will give you a hand\nyou can grab\non walls to execute a \nwall jump.\nenemies ahead\ntoo hahaha!", "Recommended use of\nslide to proceed above,\nrun and hold S to\nexecute the slide.",
            "Use dash by pressing\nW when moving in the air\nto continue", "You can now use the\nslide by pressing S while\nrunning!",
            "When crouching you stay\nintangible during\na certain time!"};
        static const char* pt[] = {"", "Olá humana..\nVamos jogar um jogo?\nTente fugir do lar \nque vocês humanos\ninvadiram e se conseguir \neu deixo você viver...\nBoa sorte.",
            "Vou te dar uma ajuda\nvocê pode se agarrar \nem paredes para executar\num walljump.\nAh inimigos a frente \ntambem hahaha!", "Recomendado o uso do \nslide para prosseguir acima,\ncorra e segure S para \nexecutar o slide.",
            "Use o dash apertando\nW ao se mover no ar \npara prosseguir", "Você agora pode usar o \nslide apertando S enquanto \ncorre!",
            "Ao agachar voce fica\nintangivel durante\ncerto tempo!"};
        if (player_.dropThrough && player_.hsp == 0.0F) {
#ifdef __3DS__
            lowerSignMessage=(language_?pt:en)[activeSign_];
#else
            drawSprite("spr_popup",0,192,109,1,1,0,0xFFFFFFFF);
            const char* message=(language_?pt:en)[activeSign_];
            constexpr float dialogCenterX=192.0F,dialogCenterY=109.0F,textSize=8.0F;
            int lineCount=1;
            for(const char* cursor=message;*cursor;++cursor) if(*cursor=='\n') ++lineCount;
            float lineY=dialogCenterY-lineCount*textSize/2.0F;
            const char* line=message;
            while(*line) {
                const char* end=std::strchr(line,'\n');
                const std::string value(line,end?static_cast<std::size_t>(end-line):std::strlen(line));
                text(value.c_str(),dialogCenterX-textWidth(value.c_str(),textSize)/2.0F,
                     lineY,textSize,BLACK);
                lineY+=textSize;
                if(!end) break;
                line=end+1;
            }
#endif
        } else if (!player_.dropThrough) {
            // Use the same interpolated anchor as the rendered player. This
            // keeps the prompt locked to the exact horizontal centre of Elli
            // while the camera moves, with the 12x12 button above her head.
            const Vector2 pos=GetWorldToScreen2D({
                mix(player_.previousX,player_.x,renderAlpha_),
                mix(player_.previousY,player_.y,renderAlpha_)-25.0F},camera_);
            const auto alpha=static_cast<std::uint32_t>(std::lround(signPromptAlpha_*255.0F));
            drawSprite("spr_button",0,pos.x,pos.y,1,1,0,(alpha<<24)|0x00FFFFFFU);
#ifdef __3DS__
            constexpr const char* promptKey="V";
#else
            constexpr const char* promptKey="S";
#endif
            constexpr float promptSize=8.0F;
            text(promptKey,pos.x-textWidth(promptKey,promptSize)/2.0F,
                 pos.y-promptSize/2.0F,promptSize,Fade(BLACK,signPromptAlpha_));
        }
    }
    if (room_->name == "rm_boss" && hasPlayer_ && !player_.dead) {
#ifndef __3DS__
        drawSprite("spr_bossbar_back", 0, 50, 185, 1, 1, 0, 0xFFFFFFFF);
        DrawRectangle(68, 192, static_cast<int>(std::max(0.0F, bossLife_-68.0F)), 13, gmColour(6247869));
        drawSprite("spr_bossbar", 0, 50, 185, 1, 1, 0, 0xFFFFFFFF);
#endif
    }
    if (room_->name == "rm_end") {
        drawSprite(trueEnding_ ? "spr_trueend" : "spr_end", 0,
                   trueEnding_ ? endingAnim_ : 0.0F, 0, 1, 1, 0, 0xFFFFFFFF);
    }
    if (room_->name == "rm_language" || room_->name == "rm_menu") {
        const bool languageScreen = room_->name == "rm_language";
        if (!languageScreen) {
            // obj_title is part of the menu presentation (not the playable
            // world), so compose it with the other menu elements.
            const int phase=(roomStepCounter_+1)%80;
            const float angle = phase < 20 ? phase*0.25F
                : phase < 60 ? 5.0F-(phase-20)*0.25F
                             : -5.0F+(phase-60)*0.25F;
            // With menu interaction on the lower display, the original title
            // art occupies the exact centre of the upper logical viewport.
#ifdef __3DS__
            drawSprite("spr_title",0,192,109,1,1,angle,0xFFFFFFFF);
#else
            drawSprite("spr_title",0,193,63,1,1,angle,0xFFFFFFFF);
#endif
        }
        const char* enMenu[] = {"Play", "Play+", "Credits", "Quit"};
        const char* ptMenu[] = {"Jogar", "Jogar+", "Créditos", "Sair"};
        const char* languages[] = {"English", "PT-BR"};
        const int count = languageScreen ? 2 :
#ifdef __3DS__
            3;
#else
            4;
#endif
        const int centerX = languageScreen ? 151 : 192;
        const int centerY = languageScreen ? 96 : 128;
        bool upperControls=true;
#ifdef __3DS__
        upperControls=languageScreen;
#endif
        if(upperControls) {
            for (int i = 0; i < count; ++i) {
                const char* label = languageScreen ? languages[i] : (language_ ? ptMenu[i] : enMenu[i]);
                const int y = centerY + i * 24;
                const Color menuColor = (!languageScreen && i == 1 && !gameCompleted_) ? GRAY : RAYWHITE;
                text(label, centerX - textWidth(label, 12)/2, (float)y-6, 12, menuColor);
            }
            drawSprite("spr_arrow", 0, (float)centerX-51, (float)(centerY+menuSelection_*24), 1, 1, 0, 0xFFFFFFFF);
            if (languageScreen) drawSprite(menuSelection_ ? "spr_brazil" : "spr_usa", 0, 231, (float)(96+menuSelection_*24), 1, 1, 0, 0xFFFFFFFF);
        }
        if (!languageScreen) {
#ifndef __3DS__
            if (!gameCompleted_) drawSprite("spr_cross", 0, 192, 151, 1, 1, 0, 0xFFFFFFFF);
#endif
            if (gameCompleted_) {
                text(language_ ? "Melhor tempo:" : "Best time:", 22, 125, 8);
                const int last=static_cast<int>(std::round((bestFrames_%60)*1.66666666667F));
                text(TextFormat("%d:%02d:%02d",bestFrames_/3600,(bestFrames_/60)%60,last), 25, 138, 8);
            }
            text("v1.2", 360, 210, 4);
        }
    } else if (room_->name == "rm_credits") {
#ifndef __3DS__
        const char* lines = language_ ? "Programado por Annie\n\nGraficos por Pavao Gripado e IGustaMe\n\nMusicas por BainoLOL\n\nObrigado por jogar!"
                                      : "Programmed by Annie\n\nGraphics by Pavao Gripado and IGustaMe\n\nMusic by BainoLOL\n\nThanks for playing!";
        text(lines, 72, 58, 8);
#endif
    }
    if (paused_) {
#ifndef __3DS__
        DrawRectangle(0, 0, kViewWidth, kViewHeight, Fade(BLACK, 0.75F));
        drawSprite("spr_pause", (float)language_, 190, 110, 1, 1, 0, 0xFFFFFFFF);
#ifdef __PSP__
        const char* en[] = {"Resume","Reset","Menu",""};
        const char* pt[] = {"Continuar","Reiniciar","Menu",""};
        static constexpr float pauseRows[]={72,96,120,168};
        constexpr int pauseItems=4;
#else
        const char* en[] = {"Resume","Reset","Fullscreen","Menu",""};
        const char* pt[] = {"Continuar","Reiniciar","Tela cheia","Menu",""};
        constexpr int pauseItems=5;
#endif
        for (int i=0;i<pauseItems;++i) {
            const char* label = language_ ? pt[i] : en[i];
#ifdef __PSP__
            const float row=pauseRows[i];
#else
            const float row=72+i*24;
#endif
            text(label, 192-textWidth(label,12)/2, row-6, 12);
        }
#ifdef __PSP__
        drawSprite("spr_longarrow", 0, 129, pauseRows[pauseSelection_], 1, 1, 0, 0xFFFFFFFF);
#else
        drawSprite("spr_longarrow", 0, 129, (float)(72+pauseSelection_*24), 1, 1, 0, 0xFFFFFFFF);
#endif
        drawSprite("spr_sound", muted_ ? 1.0F : 0.0F, 192, 167, 1, 1, 0, 0xFFFFFFFF);
#endif
    }
    if (room_->name == "rm_end" && trueEnding_ && sceneTimer_ > 550)
        DrawRectangle(0,0,kViewWidth,kViewHeight,Fade(BLACK,std::min(1.0F,(sceneTimer_-550)/10.0F)));
    if (roomFadeFrames_ > 0) DrawRectangle(0,0,kViewWidth,kViewHeight,
        Fade(BLACK,roomFadeFrames_/(float)std::max(1,roomFadeMax_)));
    if (transitioning_) DrawRectangle(0,0,kViewWidth,kViewHeight,Fade(BLACK,std::min(1.0F,transitionTimer_/10.0F)));
    if (player_.dead && player_.deathTimer > 85)
        DrawRectangle(0,0,kViewWidth,kViewHeight,Fade(BLACK,std::min(1.0F,(player_.deathTimer-85)/10.0F)));
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    const float scale =
#if defined(__3DS__) || defined(__PSP__)
        1.0F;
#else
        std::min(GetScreenWidth() / (float)kViewWidth, GetScreenHeight() / (float)kViewHeight);
#endif
    const float width = kViewWidth * scale, height = kViewHeight * scale;
    DrawTexturePro(target_.texture, {0, 0, (float)target_.texture.width, -(float)target_.texture.height},
                   {(GetScreenWidth()-width)/2, (GetScreenHeight()-height)/2, width, height}, {0,0}, 0, WHITE);
#ifdef __3DS__
    // Keep gameplay exclusively on the stereoscopic upper display. The lower
    // display uses the original menu assets and accepts native touch input.
    auto& lowerBg=texture("bg_menu2").frames;
    auto& lowerChocolate=texture("spr_chocolat").frames;
    auto& lowerSkull=texture("spr_cavera").frames;
    auto& lowerArrow=texture("spr_arrow").frames;
    auto& lowerCross=texture("spr_cross").frames;
    auto& lowerLongArrow=texture("spr_longarrow").frames;
    auto& lowerPause=texture("spr_pause").frames;
    auto& lowerSound=texture("spr_sound").frames;
    auto& lowerPopup=texture("spr_popup").frames;
    auto& lowerBossBack=texture("spr_bossbar_back").frames;
    auto& lowerBoss=texture("spr_bossbar").frames;
    if (!lowerBg.empty() && !lowerChocolate.empty() && !lowerSkull.empty())
        Draw3DSBottomHud(lowerBg[0],lowerChocolate[0],lowerSkull[0],
            lowerArrow.empty()?Texture2D{}:lowerArrow[0],lowerCross.empty()?Texture2D{}:lowerCross[0],
            lowerLongArrow.empty()?Texture2D{}:lowerLongArrow[0],
            lowerPause.empty()?Texture2D{}:lowerPause[std::min<std::size_t>(language_,lowerPause.size()-1)],
            lowerSound.empty()?Texture2D{}:lowerSound[std::min<std::size_t>(muted_?1:0,lowerSound.size()-1)],
            lowerPopup.empty()?Texture2D{}:lowerPopup[0],
            lowerBossBack.empty()?Texture2D{}:lowerBossBack[0],
            lowerBoss.empty()?Texture2D{}:lowerBoss[0],
            font_,chocolates_,deaths_,
            paused_ ? 2 : (room_->name=="rm_menu" ? 1 : (room_->name=="rm_credits" ? 3 : 0)),
            paused_ ? pauseSelection_ : menuSelection_,gameCompleted_,language_,muted_,
            lowerSignMessage,room_->name=="rm_boss"&&hasPlayer_&&!player_.dead,bossLife_,gmColour(6247869));
#endif
    EndDrawing();
    camera_.target=logicalCameraTarget;
}
