#pragma once

#include "model.hpp"
#include "raylib.h"

#include <filesystem>
#include <unordered_map>

class Game {
public:
    Game(GameData data, std::filesystem::path root);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    void run();

private:
    struct Player {
        float x{}, y{}, hsp{}, vsp{}, animation{}, facing{1};
        float previousX{}, previousY{};
        int coyote{}, wallCoyote{}, wallCoyoteSide{}, jumpBuffer{}, dashFrames{}, blinkFrames{}, slopeAnimationGrace{}, slideGroundGrace{};
        int deathTimer{};
        int wallLock{};
        int blinkSoundCooldown{};
        float forcedMove{};
        bool grounded{}, dashReady{true}, dead{}, dropThrough{}, sliding{}, lowProfile{};
        std::string sprite{"spr_player_idle"};
    };
    struct TextureFrames { std::vector<Texture2D> frames; };
    struct CollisionMask { int width{}, height{}; std::vector<unsigned char> solid; };
    enum class DrawKind : unsigned char { Background, Graphic, Instance, Player };
    struct DrawItem { int depth{}; DrawKind kind{}; std::size_t index{}; };
    struct InputPulse {
        bool up{}, down{}, confirm{}, jump{}, click{}, escape{}, restart{}, collision{};
        Vector2 mouse{-1000,-1000};
    };

    GameData data_;
    std::filesystem::path root_;
    std::unordered_map<std::string, TextureFrames> textures_;
    std::unordered_map<std::string, CollisionMask> masks_;
    std::unordered_map<std::string, std::vector<InstanceDef>> roomTemplates_;
    std::unordered_map<std::string, Sound> sounds_;
    Music music_{};
    bool musicLoaded_{};
    std::string currentMusic_;
    Font font_{};
    bool fontLoaded_{};
    std::size_t roomIndex_{3};
    RoomDef* room_{};
    Player player_;
    Camera2D camera_{};
    Vector2 previousCameraTarget_{};
    RenderTexture2D target_{};
    RenderTexture2D lightTarget_{};
    bool showCollision_{};
    bool paused_{};
    bool hasPlayer_{};
    bool running_{true};
    std::string screenshotPath_;
    int renderedFrames_{}, screenshotFrame_{10};
    std::uint64_t stepCounter_{}, roomStepCounter_{};
    float qaHorizontal_{};
    bool qaDown_{};
    int qaJumpPeriod_{};
    int sceneTimer_{}, menuSelection_{}, pauseSelection_{}, language_{};
    bool muted_{};
    int roomStartChocolates_{};
    bool haveSlide_{}, haveDash_{}, haveBlink_{}, plusMode_{};
    bool gameCompleted_{};
    int skin_{-1}, elapsedFrames_{}, bestFrames_{};
    int activeSign_{};
    float signPromptAlpha_{};
    int plusItems_{};
    bool trueEnding_{};
    float endingAnim_{};
    bool transitioning_{};
    int transitionTimer_{}, roomFadeFrames_{}, roomFadeMax_{10}, titleFrames_{};
    float bossLife_{317.892298784F};
    int bossState_{}, bossTimer_{}, bossTiming_{60}, bossAttackTimer_{}, bossEyeTimer_{}, bossDeathTimer_{};
    int chocolates_{}, deaths_{};
    std::vector<DrawItem> drawScratch_;
    std::vector<Rectangle> graphicBounds_;
    std::vector<std::vector<std::size_t>> graphicBuckets_;
    std::vector<std::uint32_t> graphicVisitStamp_;
    std::vector<std::size_t> visibleGraphicScratch_;
    std::uint32_t graphicQueryStamp_{1};
    int graphicGridColumns_{}, graphicGridRows_{};
    std::vector<std::size_t> collisionIndices_;
    std::vector<std::size_t> slopeIndices_;
    std::vector<std::size_t> simulationIndices_;
    std::vector<std::size_t> animationIndices_;
    std::vector<std::size_t> hazardIndices_;
    std::vector<std::size_t> lightIndices_;
    bool roomLighting_{};
    std::size_t indexedInstanceCount_{};
    std::vector<Vector2> sparkleScratch_;
    std::vector<InstanceDef> spawnScratch_;
    InputPulse input_{};
    float renderAlpha_{1.0F};
    std::uint64_t drawListStep_{};
    int drawListCameraCellX_{}, drawListCameraCellY_{};
    bool drawListValid_{};
    double previousFrameTime_{}, frameAccumulator_{1.0/45.0};

    void loadRoom(std::size_t index, bool restart = false);
    void preloadRoomAssets();
    void rebuildGraphicIndex();
    void rebuildCollisionIndex();
    void rebuildDrawList();
    void update();
    void runFrame();
    void pollInput();
    void snapshotForInterpolation();
    void clearInputPulse();
    void updatePlayer();
    void updateInstances();
    void updateBoss();
    InstanceDef makeInstance(const std::string& objectName, float x, float y) const;
    void draw();
    void drawWorld();
    void drawLighting();
    void drawSprite(const std::string& name, float frame, float x, float y, float scaleX,
                    float scaleY, float rotation, std::uint32_t tint, const Rectangle* source = nullptr);
    const SpriteDef* sprite(const std::string& name) const;
    const ObjectDef* object(const std::string& name) const;
    TextureFrames& texture(const std::string& name);
    bool inherits(const std::string& objectName, const std::string& ancestor) const;
    Rectangle instanceBounds(const InstanceDef& instance) const;
    bool playerUsesLowProfile() const;
    Rectangle playerBounds(float x, float y) const;
    Rectangle playerBoundsWithProfile(float x, float y, bool lowProfile) const;
    const CollisionMask& mask(const std::string& spriteName, float frame = 0);
    bool instanceCollision(const Rectangle& bounds, const InstanceDef& instance);
    bool collides(float x, float y);
    bool collidesWithProfile(float x, float y, bool lowProfile);
    bool slopeBelow(float distance);
    void moveAxis(float& coordinate, float amount, bool horizontal);
    bool touchesObject(const std::string& name);
    bool touchesHazard();
    Color gmColour(std::uint32_t value) const;
    void playSound(const std::string& name, float volume = 1.0F);
    void setRoomMusic();
    void preloadNextRoomMusic();
    void loadSave();
    void saveGame() const;
    std::string playerSprite(const std::string& normal) const;
    void loadFont();
    void text(const char* value, float x, float y, float size, Color color = RAYWHITE) const;
    float textWidth(const char* value, float size) const;
};
