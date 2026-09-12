#pragma once

#include <cstddef>
#include <cstdint>

struct Vector2 { float x, y; };
struct Rectangle { float x, y, width, height; };
struct Color { unsigned char r, g, b, a; };
struct Texture2D { unsigned int id; int width, height, mipmaps, format; };
struct Image { void* data; int width, height, mipmaps, format; };
struct Sound { unsigned int id; };
struct Music { unsigned int id; bool looping; };
struct GlyphInfo { int value, offsetX, offsetY, advanceX; Image image; };
struct Font { int baseSize, glyphCount, glyphPadding; Texture2D texture; Rectangle* recs; GlyphInfo* glyphs; };
struct RenderTexture2D { unsigned int id; Texture2D texture, depth; };
struct Camera2D { Vector2 offset, target; float rotation, zoom; };

inline constexpr Color WHITE{255,255,255,255}, BLACK{0,0,0,255}, RED{230,41,55,255};
inline constexpr Color LIME{0,255,0,255}, GRAY{130,130,130,255}, RAYWHITE{245,245,245,255};
inline constexpr float PI=3.14159265358979323846F, RAD2DEG=180.0F/PI;

enum {
    FLAG_VSYNC_HINT=0x40, FLAG_WINDOW_RESIZABLE=0x4,
    KEY_NULL=0, KEY_SPACE=32, KEY_A=65, KEY_D=68, KEY_F1=290, KEY_F10=299,
    KEY_W=87, KEY_S=83, KEY_R=82, KEY_ENTER=257, KEY_ESCAPE=256,
    KEY_RIGHT=262, KEY_LEFT=263, KEY_DOWN=264, KEY_UP=265, KEY_LEFT_SHIFT=340,
    MOUSE_BUTTON_LEFT=0,
    GAMEPAD_BUTTON_LEFT_FACE_UP=1, GAMEPAD_BUTTON_LEFT_FACE_RIGHT=2,
    GAMEPAD_BUTTON_LEFT_FACE_DOWN=3, GAMEPAD_BUTTON_LEFT_FACE_LEFT=4,
    GAMEPAD_BUTTON_RIGHT_FACE_UP=5, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT=6,
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN=7, GAMEPAD_BUTTON_RIGHT_FACE_LEFT=8,
    GAMEPAD_BUTTON_LEFT_TRIGGER_1=9, GAMEPAD_BUTTON_RIGHT_TRIGGER_1=11,
    GAMEPAD_BUTTON_MIDDLE_RIGHT=13, GAMEPAD_AXIS_LEFT_X=0, GAMEPAD_AXIS_LEFT_Y=1,
    TEXTURE_FILTER_POINT=0, BLEND_CUSTOM_SEPARATE=7,
    LOG_INFO=1, LOG_WARNING=2
};

void SetConfigFlags(unsigned int); void ClearWindowState(unsigned int);
void InitWindow(int,int,const char*); void CloseWindow(); bool WindowShouldClose();
void SetExitKey(int); void SetTargetFPS(int); int GetScreenWidth(); int GetScreenHeight();
double GetTime(); void BeginDrawing(); void EndDrawing(); void ClearBackground(Color);
void BeginMode2D(Camera2D); void EndMode2D(); Vector2 GetWorldToScreen2D(Vector2,Camera2D);
RenderTexture2D LoadRenderTexture(int,int); void UnloadRenderTexture(RenderTexture2D);
void BeginTextureMode(RenderTexture2D); void EndTextureMode();
Texture2D LoadTexture(const char*); void UnloadTexture(Texture2D); void SetTextureFilter(Texture2D,int);
Image LoadImage(const char*); Color* LoadImageColors(Image); void UnloadImageColors(Color*); void UnloadImage(Image);
void DrawTexturePro(Texture2D,Rectangle,Rectangle,Vector2,float,Color);
void DrawTextureRec(Texture2D,Rectangle,Vector2,Color);
void DrawRectangle(int,int,int,int,Color); void DrawRectangleLinesEx(Rectangle,float,Color);
void DrawCircleGradient(int,int,float,Color,Color);
bool CheckCollisionRecs(Rectangle,Rectangle);
void BeginBlendMode(int); void EndBlendMode(); Color Fade(Color,float);
void* MemAlloc(unsigned int); void UnloadFont(Font); void DrawText(const char*,int,int,int,Color);
void DrawTextEx(Font,const char*,Vector2,float,float,Color); int MeasureText(const char*,int);
Vector2 MeasureTextEx(Font,const char*,float,float); const char* TextFormat(const char*,...);
bool IsKeyDown(int); bool IsKeyPressed(int); bool IsGamepadButtonDown(int,int);
bool IsGamepadButtonPressed(int,int); float GetGamepadAxisMovement(int,int);
bool IsMouseButtonPressed(int); Vector2 GetMousePosition();
int GetRandomValue(int,int); void TraceLog(int,const char*,...); void TakeScreenshot(const char*);
void InitAudioDevice(); void CloseAudioDevice(); void SetMasterVolume(float);
Sound LoadSound(const char*); void UnloadSound(Sound); bool IsSoundValid(Sound);
void SetSoundVolume(Sound,float); void PlaySound(Sound);
Music LoadMusicStream(const char*); void UnloadMusicStream(Music); bool IsMusicValid(Music);
void PlayMusicStream(Music); void StopMusicStream(Music); void PauseMusicStream(Music);
void ResumeMusicStream(Music); void UpdateMusicStream(Music); void SetMusicVolume(Music,float);
void ToggleFullscreen();
// Selects stereoscopic disparity for subsequent upper-screen draw commands.
// 0 is the far background and 1 is the nearest UI plane.
void Set3DStereoLayer(float depth);
// Decodes one anticipated music stream on a worker while the current room runs.
void PreloadMusicStream(const char* path);

// 3DS-only compositor: persistent statistics live on the lower screen.
void Draw3DSBottomHud(Texture2D background, Texture2D chocolate, Texture2D skull,
                      Texture2D arrow, Texture2D cross, Texture2D longArrow,
                      Texture2D pauseArt, Texture2D soundIcon, Texture2D popup,
                      Texture2D bossbarBack, Texture2D bossbar,
                      Font font, int chocolates, int deaths, int mode,
                      int selection, bool gameCompleted, int language, bool muted,
                      const char* signMessage, bool showBoss, float bossLife,
                      Color bossFill);
