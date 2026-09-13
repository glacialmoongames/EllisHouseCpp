#include "raylib.h"
#include "texture_map.hpp"
#define KEY_A CTR_KEY_A
#define KEY_R CTR_KEY_R
#define KEY_UP CTR_KEY_UP
#define KEY_DOWN CTR_KEY_DOWN
#define KEY_LEFT CTR_KEY_LEFT
#define KEY_RIGHT CTR_KEY_RIGHT
#include <3ds.h>
#undef KEY_A
#undef KEY_R
#undef KEY_UP
#undef KEY_DOWN
#undef KEY_LEFT
#undef KEY_RIGHT
#include <citro2d.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct Sheet { C2D_SpriteSheet handle{}; std::uint64_t lastUsedFrame{}; };
enum class CommandKind : unsigned char { Texture, Rectangle, Line, Circle, BlendSubtract, BlendNormal };
struct ScreenCommand {
    CommandKind kind{CommandKind::Texture}; Texture2D tex{}; Rectangle src{},dst{};
    Vector2 origin{}; float rotation{},thickness{}; Color tint{},tint2{};
    Camera2D camera{}; bool useCamera{}; float stereoLayer{0.55f};
};
struct Rt {
    C3D_Tex tex{}; C3D_RenderTarget* target{}; Tex3DS_SubTexture sub{}; C2D_Image image{};
    int w{},h{}; Color clear{BLACK}; std::vector<ScreenCommand> commands;
};
struct AudioClip { s16* data{}; u32 frames{}; float volume{1.0f}; bool music{}; };
std::vector<Sheet> sheets;
std::vector<Rt*> targets;
std::vector<AudioClip*> audioClips;
ndspWaveBuf channelWaves[24]{};
unsigned channelClip[24]{};
int nextSfxChannel=1;
float masterGain=1.0f;
bool audioReady=false;
Thread musicPreloadThread{};
int musicWorkerCore=-2;
std::atomic<bool> musicPreloadDone{false};
std::string musicPreloadPath;
AudioClip* musicPreloadClip{};
C3D_RenderTarget *topLeft{},*topRight{},*bottom{};
Rt* currentRt{};
Camera2D camera{};
bool initialized=false, frame=false, mode2d=false, screenDrawing=false;
u32 held{},down{};
double startTime{};
std::uint64_t frameSerial{};
Color screenClear=BLACK;
float eyeShift=0.0f;
float viewOffsetX=0.0f,viewOffsetY=0.0f;
float stereoLayer=0.55f;

u32 color32(Color c) { return C2D_Color32(c.r,c.g,c.b,c.a); }
std::string normalize(const char* p) { std::string s=p?p:""; std::replace(s.begin(),s.end(),'\\','/'); return s; }
int assetIndex(const char* p) {
    static std::unordered_map<std::string,int> indices;
    if(indices.empty()) {
        indices.reserve(gTextureAssetCount3DS*2);
        for(std::size_t i=0;i<gTextureAssetCount3DS;i++) {
            const std::string mapped=normalize(gTextureAssets3DS[i].path);
            indices.emplace(mapped,(int)i);
            const auto marker=mapped.find("assets/");
            if(marker!=std::string::npos)indices.emplace(mapped.substr(marker),(int)i);
        }
    }
    const auto full=normalize(p);
    const auto marker=full.find("assets/");
    const std::string relative=marker==std::string::npos?full:full.substr(marker);
    if(const auto found=indices.find(full);found!=indices.end())return found->second;
    if(const auto found=indices.find(relative);found!=indices.end())return found->second;
    return -1;
}
void ensureFrame() { if(!frame) { C3D_FrameBegin(C3D_FRAME_SYNCDRAW); frame=true; } }
void beginTarget(C3D_RenderTarget* target) { ensureFrame(); C2D_SceneBegin(target); C2D_ViewReset(); }
C2D_SpriteSheet sheet(int id) {
    if(id<0) return nullptr; if((int)sheets.size()<=id) sheets.resize(id+1);
    if(!sheets[id].handle) sheets[id].handle=C2D_SpriteSheetLoad(gTextureSheets3DS[id]);
    if(sheets[id].handle) sheets[id].lastUsedFrame=frameSerial;
    return sheets[id].handle;
}
void trimSheetCache() {
    // Leave enough VRAM for both 512x256 RGBA8 render targets on OLD 3DS.
    // Seven RGBA5551 sheets avoid allocation stalls without evicting the
    // visible kitchen tileset working set.
    constexpr int maxResidentSheets=7;
    int resident=0;
    for(const auto& item:sheets)if(item.handle)++resident;
    while(resident>maxResidentSheets) {
        int oldest=-1;
        for(std::size_t i=0;i<sheets.size();++i) {
            if(!sheets[i].handle || sheets[i].lastUsedFrame+1>=frameSerial)continue;
            if(oldest<0 || sheets[i].lastUsedFrame<sheets[(std::size_t)oldest].lastUsedFrame)oldest=(int)i;
        }
        if(oldest<0)break;
        C2D_SpriteSheetFree(sheets[(std::size_t)oldest].handle);
        sheets[(std::size_t)oldest]={};
        --resident;
    }
}
void transform(float& x,float& y,float& sx,float& sy) {
    if(!mode2d) return;
    sx*=camera.zoom; sy*=camera.zoom;
    x=(x-camera.target.x)*camera.zoom+camera.offset.x;
    y=(y-camera.target.y)*camera.zoom+camera.offset.y;
}
void drawAsset(Texture2D texture, Rectangle src, Rectangle dst, Vector2 origin, float rotation, Color tint) {
    if(!texture.id || (texture.id&0x80000000u)) return;
    const auto& a=gTextureAssets3DS[texture.id-1];
    const float sw=std::abs(src.width),sh=std::abs(src.height); if(sw<=0||sh<=0) return;
    const bool flipX=src.width<0,flipY=src.height<0;
    int firstPart=0,lastPart=a.partCount;
    // Large GameMaker tilesets are split into regular 512px pieces. Nearly
    // every legacy tile is wholly inside one piece; select it directly rather
    // than testing all eight kitchen chunks for every 16px tile and both eyes.
    if(a.partCount>1 && sw<=512.0f && sh<=512.0f && src.x>=0 && src.y>=0) {
        const int columns=(a.width+511)/512;
        const int candidate=(int)(src.y/512.0f)*columns+(int)(src.x/512.0f);
        if(candidate>=0 && candidate<a.partCount) {
            const auto& part=gTextureParts3DS[a.firstPart+candidate];
            if(src.x+sw<=part.x+part.width+0.001f && src.y+sh<=part.y+part.height+0.001f) {
                firstPart=candidate;lastPart=candidate+1;
            }
        }
    }
    for(int n=firstPart;n<lastPart;n++) {
        const auto& p=gTextureParts3DS[a.firstPart+n];
        const float ix=std::max(src.x,(float)p.x),iy=std::max(src.y,(float)p.y);
        const float ir=std::min(src.x+sw,(float)(p.x+p.width)),ib=std::min(src.y+sh,(float)(p.y+p.height));
        if(ir<=ix||ib<=iy) continue;
        auto shandle=sheet(p.sheet); if(!shandle) continue;
        C2D_Image base=C2D_SpriteSheetGetImage(shandle,p.image);
        Tex3DS_SubTexture sub=*base.subtex;
        const float lx=ix-p.x,ly=iy-p.y,rw=ir-ix,rh=ib-iy;
        // tex3ds marks rotated atlas entries with top < bottom. Full images and
        // the overwhelmingly common tile draws need no UV surgery.
        if(lx!=0||ly!=0||rw!=p.width||rh!=p.height) {
            if(sub.top>=sub.bottom) {
                const float du=(sub.right-sub.left)/p.width,dv=(sub.bottom-sub.top)/p.height;
                sub.left+=du*lx; sub.right=sub.left+du*rw; sub.top+=dv*ly; sub.bottom=sub.top+dv*rh;
            }
            sub.width=(u16)std::max(1L,std::lround(rw));
            sub.height=(u16)std::max(1L,std::lround(rh));
        }
        // Flip the UVs, not the geometry.  Negative Citro2D dimensions make
        // the per-part rotation centre ambiguous and used to make mirrored,
        // multi-part sprites (notably the left-facing staircases) disappear.
        if(flipX) std::swap(sub.left,sub.right);
        if(flipY) std::swap(sub.top,sub.bottom);
        C2D_Image image{base.tex,&sub};
        const float ox=flipX?(src.x+sw-ir):(ix-src.x), oy=flipY?(src.y+sh-ib):(iy-src.y);
        float px=dst.x,py=dst.y;
        const float pw=rw*dst.width/sw,ph=rh*dst.height/sh;
        float scaleX=pw/rw,scaleY=ph/rh;
        // Every atlas part shares the same GameMaker instance pivot.  px/py
        // must remain at that pivot; adding the part offset here as well as
        // subtracting it from centre applied the offset twice.  That shifted
        // large scenery chunks and made sprite origins disagree with PC.
        float centreX=origin.x-ox*dst.width/sw,centreY=origin.y-oy*dst.height/sh;
        if(mode2d){centreX*=camera.zoom;centreY*=camera.zoom;}
        transform(px,py,scaleX,scaleY); px+=eyeShift+viewOffsetX; py+=viewOffsetY;
        C2D_DrawParams params{{px,py,std::abs(scaleX)*rw,std::abs(scaleY)*rh},
            {centreX,centreY},0.5f,(float)C3D_AngleFromDegrees(rotation)};
        C2D_ImageTint ti; C2D_PlainImageTint(&ti,color32(tint),1.0f);
        C2D_DrawImage(image,&params,&ti);
    }
}
void drawRender(Texture2D texture, Rectangle, Rectangle dst, Color tint, float eyeOffset) {
    const unsigned index=(texture.id&0x7fffffffu)-1; if(index>=targets.size()) return;
    Rt* rt=targets[index]; C2D_ImageTint ti; C2D_PlainImageTint(&ti,color32(tint),1.0f);
    C2D_DrawImageAt(rt->image,dst.x+eyeOffset+viewOffsetX,dst.y+viewOffsetY,0.5f,&ti,dst.width/rt->w,dst.height/rt->h);
}
// Citro2D accepts equal-depth fragments in submission order. Keeping every 2D
// primitive on the same plane is essential here: GameMaker already sorted the
// draw list, and a larger Z would place the room's fallback colour in front of
// all sprites regardless of that ordering.
constexpr float kPlaneDepth=0.5f;
void drawRectNow(Rectangle r,Color c) { float sx=1,sy=1,x=r.x,y=r.y;transform(x,y,sx,sy);C2D_DrawRectSolid(x+eyeShift+viewOffsetX,y+viewOffsetY,kPlaneDepth,r.width*sx,r.height*sy,color32(c)); }
void drawLineNow(Rectangle r,float q,Color c) { float sx=1,sy=1,x=r.x,y=r.y;transform(x,y,sx,sy);C2D_DrawLine(x+eyeShift+viewOffsetX,y+viewOffsetY,color32(c),x+r.width*sx+eyeShift+viewOffsetX,y+r.height*sy+viewOffsetY,color32(c),q,kPlaneDepth); }
void drawCircleNow(Rectangle r,Color a,Color b) {
    float sx=1,sy=1,x=r.x,y=r.y;transform(x,y,sx,sy);
    // The PICA ellipse helper shades its bounding quad before the custom
    // subtract blend on some drivers, which exposes a square light.  A small
    // triangle fan gives the blend stage real circular geometry and a smooth
    // centre-to-edge gradient on hardware and emulator alike.
    constexpr int segments=32;
    constexpr float tau=6.2831853071795864769f;
    const float cx=x+eyeShift+viewOffsetX,cy=y+viewOffsetY;
    const float radius=r.width*std::abs(sx);
    const u32 centre=color32(a),edge=color32(b);
    for(int n=0;n<segments;n++) {
        const float angle0=tau*n/segments,angle1=tau*(n+1)/segments;
        C2D_DrawTriangle(cx,cy,centre,
            cx+std::cos(angle0)*radius,cy+std::sin(angle0)*radius,edge,
            cx+std::cos(angle1)*radius,cy+std::sin(angle1)*radius,edge,kPlaneDepth);
    }
}
void replayMain(float shift) {
    if(targets.empty()) return;
    eyeShift=0; viewOffsetX=8.0f; viewOffsetY=11.0f;
    for(const auto& c:targets[0]->commands) {
        camera=c.camera; mode2d=c.useCamera; eyeShift=shift*c.stereoLayer;
        switch(c.kind) {
        case CommandKind::Texture:
            // World sprites are replayed directly; the secondary target is
            // only the small lighting mask composed over that visible world.
            if(c.tex.id&0x80000000u) drawRender(c.tex,c.src,c.dst,c.tint,eyeShift);
            else drawAsset(c.tex,c.src,c.dst,c.origin,c.rotation,c.tint);
            break;
        case CommandKind::Rectangle: drawRectNow(c.dst,c.tint); break;
        case CommandKind::Line: drawLineNow(c.dst,c.thickness,c.tint); break;
        case CommandKind::Circle: drawCircleNow(c.dst,c.tint,c.tint2); break;
        case CommandKind::BlendSubtract: BeginBlendMode(BLEND_CUSTOM_SEPARATE); break;
        case CommandKind::BlendNormal: EndBlendMode(); break;
        }
    }
    mode2d=false;
    // Recreate the clipping supplied by the original 384x218 render target.
    // These four bars also keep the logical image centered without scaling.
    C2D_Flush();
    C2D_DrawRectSolid(0,0,kPlaneDepth,8,240,color32(BLACK));
    C2D_DrawRectSolid(392,0,kPlaneDepth,8,240,color32(BLACK));
    C2D_DrawRectSolid(0,0,kPlaneDepth,400,11,color32(BLACK));
    C2D_DrawRectSolid(0,229,kPlaneDepth,400,11,color32(BLACK));
    eyeShift=0; viewOffsetX=0; viewOffsetY=0;
}
AudioClip* audioClip(unsigned id) { return id && id<=audioClips.size()?audioClips[id-1]:nullptr; }
AudioClip* decodeAudio(const char* path,bool music) {
    std::string file=normalize(path);
    const auto slash=file.find_last_of('/');if(slash!=std::string::npos)file=file.substr(slash+1);
    const auto dot=file.find_last_of('.');if(dot!=std::string::npos)file.resize(dot);
    for(char& c:file)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
    file="romfs:/audio/"+file+".PCM";
    FILE* input=std::fopen(file.c_str(),"rb");if(!input)return nullptr;
    std::fseek(input,0,SEEK_END);const long bytes=std::ftell(input);std::rewind(input);
    if(bytes<=0 || (bytes&1)){std::fclose(input);return nullptr;}
    auto* clip=new AudioClip;clip->frames=static_cast<u32>(bytes/sizeof(s16));clip->music=music;
    clip->data=static_cast<s16*>(linearAlloc(static_cast<std::size_t>(bytes)));
    if(!clip->data || std::fread(clip->data,1,static_cast<std::size_t>(bytes),input)!=static_cast<std::size_t>(bytes)) {
        if(clip->data)linearFree(clip->data);delete clip;clip=nullptr;
    } else DSP_FlushDataCache(clip->data,static_cast<std::size_t>(bytes));
    std::fclose(input);return clip;
}
void musicPreloadWorker(void* argument) {
    auto* path=static_cast<std::string*>(argument);
    musicPreloadClip=decodeAudio(path->c_str(),true);
    delete path;
    musicPreloadDone.store(true,std::memory_order_release);
}
void finishMusicPreload(bool wait) {
    if(!musicPreloadThread)return;
    if(!wait && !musicPreloadDone.load(std::memory_order_acquire))return;
    threadJoin(musicPreloadThread,U64_MAX);
    threadFree(musicPreloadThread);
    musicPreloadThread=nullptr;
}
void discardFinishedMusicPreload() {
    finishMusicPreload(false);
    if(musicPreloadThread || !musicPreloadClip)return;
    if(musicPreloadClip->data)linearFree(musicPreloadClip->data);
    delete musicPreloadClip;
    musicPreloadClip=nullptr;
    musicPreloadPath.clear();
    musicPreloadDone.store(false,std::memory_order_release);
}
void setChannelMix(int channel,float volume) { float mix[12]{};mix[0]=mix[1]=std::clamp(volume*masterGain,0.0f,1.0f);ndspChnSetMix(channel,mix); }
void startChannel(int channel,AudioClip* clip,bool loop) {
    if(!audioReady||!clip||!clip->data)return;ndspChnReset(channel);ndspChnSetInterp(channel,NDSP_INTERP_LINEAR);
    ndspChnSetRate(channel,22050);ndspChnSetFormat(channel,NDSP_FORMAT_MONO_PCM16);setChannelMix(channel,clip->volume);
    std::memset(&channelWaves[channel],0,sizeof(ndspWaveBuf));channelWaves[channel].data_vaddr=clip->data;
    channelWaves[channel].nsamples=clip->frames;channelWaves[channel].looping=loop;ndspChnWaveBufAdd(channel,&channelWaves[channel]);
}
void freeAudio(unsigned id) {
    auto* clip=audioClip(id);if(!clip)return;for(int c=0;c<24;c++)if(channelClip[c]==id){ndspChnReset(c);channelClip[c]=0;}
    if(clip->data)linearFree(clip->data);delete clip;audioClips[id-1]=nullptr;
}
int keyForButton(int b) { switch(b) {
 case GAMEPAD_BUTTON_LEFT_FACE_UP:return KEY_DUP; case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:return KEY_DRIGHT;
 case GAMEPAD_BUTTON_LEFT_FACE_DOWN:return KEY_DDOWN; case GAMEPAD_BUTTON_LEFT_FACE_LEFT:return KEY_DLEFT;
 case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:return CTR_KEY_A; case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:return KEY_B;
 case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:return KEY_X; case GAMEPAD_BUTTON_RIGHT_FACE_UP:return KEY_Y;
 case GAMEPAD_BUTTON_LEFT_TRIGGER_1:return KEY_L; case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:return CTR_KEY_R;
 case GAMEPAD_BUTTON_MIDDLE_RIGHT:return KEY_START; default:return 0; }}
}

void SetConfigFlags(unsigned int){} void ClearWindowState(unsigned int){}
void InitWindow(int,int,const char*) { if(initialized)return; gfxInitDefault(); gfxSet3D(true); C3D_Init(C3D_DEFAULT_CMDBUF_SIZE); C2D_Init(8192); C2D_Prepare(); C2D_SetTintMode(C2D_TintMult); topLeft=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT); topRight=C2D_CreateScreenTarget(GFX_TOP,GFX_RIGHT); bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT); sheets.resize(256); musicWorkerCore=R_SUCCEEDED(APT_SetAppCpuTimeLimit(20))?1:-2; startTime=osGetTime()/1000.0; initialized=true; }
void CloseWindow(){ if(!initialized)return; for(auto&s:sheets)if(s.handle)C2D_SpriteSheetFree(s.handle); for(auto*r:targets){if(r->target)C3D_RenderTargetDelete(r->target);if(r->tex.data)C3D_TexDelete(&r->tex);delete r;} C2D_Fini();C3D_Fini();gfxExit();initialized=false; }
bool WindowShouldClose(){
    if(!aptMainLoop())return true;
    hidScanInput();
    static u32 previousAnalog=0;
    const u32 physicalHeld=hidKeysHeld();
    const u32 physicalDown=hidKeysDown();
    circlePosition position;hidCircleRead(&position);
    u32 analog=0;
    // libctru exposes Circle Pad directions as CPAD bits, while the shared
    // raylib adapter expects D-pad bits. Normalize the hardware bits first;
    // the raw-axis fallback also covers small differences in HID calibration.
    if(physicalHeld&KEY_CPAD_LEFT)analog|=KEY_DLEFT;
    if(physicalHeld&KEY_CPAD_RIGHT)analog|=KEY_DRIGHT;
    if(physicalHeld&KEY_CPAD_DOWN)analog|=KEY_DDOWN;
    if(physicalHeld&KEY_CPAD_UP)analog|=KEY_DUP;
    if(position.dx<-40)analog|=KEY_DLEFT;
    if(position.dx>40)analog|=KEY_DRIGHT;
    if(position.dy<-40)analog|=KEY_DDOWN;
    if(position.dy>40)analog|=KEY_DUP;
    u32 analogPressed=analog&~previousAnalog;
    if(physicalDown&KEY_CPAD_LEFT)analogPressed|=KEY_DLEFT;
    if(physicalDown&KEY_CPAD_RIGHT)analogPressed|=KEY_DRIGHT;
    if(physicalDown&KEY_CPAD_DOWN)analogPressed|=KEY_DDOWN;
    if(physicalDown&KEY_CPAD_UP)analogPressed|=KEY_DUP;
    held=physicalHeld|analog;
    down=physicalDown|analogPressed;
    previousAnalog=analog;
    return false;
}
void SetExitKey(int){} void SetTargetFPS(int){} int GetScreenWidth(){return 400;} int GetScreenHeight(){return 240;}
double GetTime(){return osGetTime()/1000.0-startTime;}
void BeginDrawing(){++frameSerial;if(frame)C2D_Flush();trimSheetCache();screenDrawing=true;beginTarget(topLeft);C2D_TargetClear(topLeft,color32(screenClear));}
void EndDrawing(){
    const float depth=osGet3DSliderState()*3.5f;
    static bool stereoEnabled=true;
    const bool wantsStereo=depth>0.01f;
    // At zero slider position the hardware displays only the left framebuffer;
    // skipping an identical right-eye replay nearly halves the scene work.
    if(wantsStereo){beginTarget(topRight);C2D_TargetClear(topRight,color32(screenClear));replayMain(depth);}
    if(frame){C3D_FrameEnd(0);frame=false;}
    if(wantsStereo!=stereoEnabled){gfxSet3D(wantsStereo);stereoEnabled=wantsStereo;}
    screenDrawing=false;
}
void ClearBackground(Color c){ if(screenDrawing){screenClear=c;C2D_TargetClear(topLeft,color32(c));}else if(currentRt==targets[0]){currentRt->clear=c;currentRt->commands.clear();}else if(currentRt)C2D_TargetClear(currentRt->target,color32(c));else C2D_DrawRectSolid(0,0,0.9f,400,240,color32(c)); }
void BeginMode2D(Camera2D c){camera=c;mode2d=true;} void EndMode2D(){mode2d=false;}
Vector2 GetWorldToScreen2D(Vector2 v,Camera2D c){return {(v.x-c.target.x)*c.zoom+c.offset.x,(v.y-c.target.y)*c.zoom+c.offset.y};}
RenderTexture2D LoadRenderTexture(int w,int h){auto*r=new Rt;r->w=w;r->h=h;C3D_TexInitVRAM(&r->tex,512,256,GPU_RGBA8);C3D_TexSetFilter(&r->tex,GPU_NEAREST,GPU_NEAREST);r->target=C3D_RenderTargetCreateFromTex(&r->tex,GPU_TEXFACE_2D,0,-1);r->sub={(u16)w,(u16)h,0.0f,1.0f,w/512.0f,1.0f-h/256.0f};r->image={&r->tex,&r->sub};targets.push_back(r);unsigned id=0x80000000u|(unsigned)targets.size();return{id,{id,w,h,1,0},{}};}
void UnloadRenderTexture(RenderTexture2D){} void BeginTextureMode(RenderTexture2D t){unsigned i=(t.id&0x7fffffffu)-1;if(i<targets.size()){currentRt=targets[i];if(i!=0)beginTarget(currentRt->target);}} void EndTextureMode(){currentRt=nullptr;mode2d=false;}
Texture2D LoadTexture(const char*p){int i=assetIndex(p);return i<0?Texture2D{}:Texture2D{(unsigned)i+1,gTextureAssets3DS[i].width,gTextureAssets3DS[i].height,1,0};}
void UnloadTexture(Texture2D){} void SetTextureFilter(Texture2D,int){}
Image LoadImage(const char*p){int i=assetIndex(p);if(i<0)return{};const auto&a=gTextureAssets3DS[i];char file[64];std::snprintf(file,sizeof(file),"romfs:/masks/m%04d.a8",i);FILE*f=std::fopen(file,"rb");if(!f)return{};auto*data=(Color*)std::malloc(sizeof(Color)*a.width*a.height);for(int n=0;n<a.width*a.height;n++){int alpha=std::fgetc(f);data[n]={255,255,255,(unsigned char)(alpha<0?0:alpha)};}std::fclose(f);return{data,a.width,a.height,1,0};}
Color* LoadImageColors(Image i){if(!i.data)return nullptr;auto*p=(Color*)std::malloc(sizeof(Color)*i.width*i.height);std::memcpy(p,i.data,sizeof(Color)*i.width*i.height);return p;} void UnloadImageColors(Color*p){std::free(p);} void UnloadImage(Image i){std::free(i.data);}
void DrawTexturePro(Texture2D t,Rectangle s,Rectangle d,Vector2 o,float r,Color c){
    if(currentRt==targets[0]){currentRt->commands.push_back({CommandKind::Texture,t,s,d,o,r,0,c,{},camera,mode2d,stereoLayer});return;}
    if(screenDrawing&&(t.id&0x80000000u)){replayMain(-osGet3DSliderState()*3.5f);return;}
    if(t.id&0x80000000u)drawRender(t,s,d,c,0);else drawAsset(t,s,d,o,r,c);
}
void DrawTextureRec(Texture2D t,Rectangle s,Vector2 p,Color c){DrawTexturePro(t,s,{p.x,p.y,std::abs(s.width),std::abs(s.height)},{0,0},0,c);}
void DrawRectangle(int x,int y,int w,int h,Color c){Rectangle r{(float)x,(float)y,(float)w,(float)h};if(currentRt==targets[0]){currentRt->commands.push_back({CommandKind::Rectangle,{}, {},r,{},0,0,c,{},camera,mode2d,stereoLayer});return;}drawRectNow(r,c);}
void DrawRectangleLinesEx(Rectangle r,float q,Color c){Rectangle lines[4]={{r.x,r.y,r.width,0},{r.x,r.y+r.height,r.width,0},{r.x,r.y,0,r.height},{r.x+r.width,r.y,0,r.height}};for(auto line:lines){if(currentRt==targets[0])currentRt->commands.push_back({CommandKind::Line,{}, {},line,{},0,q,c,{},camera,mode2d,stereoLayer});else drawLineNow(line,q,c);}}
void DrawCircleGradient(int x,int y,float r,Color a,Color b){Rectangle v{(float)x,(float)y,r,0};if(currentRt==targets[0]){currentRt->commands.push_back({CommandKind::Circle,{}, {},v,{},0,0,a,b,camera,mode2d,stereoLayer});return;}drawCircleNow(v,a,b);}
bool CheckCollisionRecs(Rectangle a,Rectangle b){return a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y;}
void BeginBlendMode(int){if(!targets.empty()&&currentRt==targets[0]){currentRt->commands.push_back({CommandKind::BlendSubtract});return;}C2D_Flush();C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ZERO,GPU_ONE_MINUS_SRC_COLOR,GPU_ZERO,GPU_ONE);}
void EndBlendMode(){if(!targets.empty()&&currentRt==targets[0]){currentRt->commands.push_back({CommandKind::BlendNormal});return;}C2D_Flush();C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);}
void Set3DStereoLayer(float depth){stereoLayer=std::clamp(depth,-0.25f,1.45f);}
Color Fade(Color c,float a){c.a=(unsigned char)(std::clamp(a,0.0f,1.0f)*c.a);return c;}
void* MemAlloc(unsigned int n){return std::calloc(1,n);} void UnloadFont(Font f){std::free(f.recs);std::free(f.glyphs);}
void DrawText(const char*,int,int,int,Color){} int MeasureText(const char*s,int z){return s?(int)std::strlen(s)*z/2:0;}
int nextUtf8(const char*& text) {
    const auto first=(unsigned char)*text++;
    if(first<0x80)return first;
    int value=0,continuations=0;
    if((first&0xe0)==0xc0){value=first&0x1f;continuations=1;}
    else if((first&0xf0)==0xe0){value=first&0x0f;continuations=2;}
    else if((first&0xf8)==0xf0){value=first&0x07;continuations=3;}
    else return '?';
    for(int i=0;i<continuations;i++) {
        const auto part=(unsigned char)*text;
        if((part&0xc0)!=0x80)return '?';
        ++text;value=(value<<6)|(part&0x3f);
    }
    return value;
}
void DrawTextEx(Font f,const char*s,Vector2 p,float size,float spacing,Color c){if(!s)return;float x=p.x,y=p.y,scale=size/f.baseSize;const char* cursor=s;while(*cursor){const int codepoint=nextUtf8(cursor);if(codepoint=='\n'){x=p.x;y+=size;continue;}int gi=-1;for(int i=0;i<f.glyphCount;i++)if(f.glyphs[i].value==codepoint){gi=i;break;}if(gi<0)continue;auto r=f.recs[gi];DrawTexturePro(f.texture,r,{x+f.glyphs[gi].offsetX*scale,y+f.glyphs[gi].offsetY*scale,r.width*scale,r.height*scale},{0,0},0,c);x+=(f.glyphs[gi].advanceX?f.glyphs[gi].advanceX:r.width)*scale+spacing;}}
Vector2 MeasureTextEx(Font f,const char*s,float size,float spacing){float x=0,max=0;int lines=1;const char* cursor=s;while(cursor&&*cursor){const int codepoint=nextUtf8(cursor);if(codepoint=='\n'){max=std::max(max,x);x=0;lines++;continue;}int gi=-1;for(int i=0;i<f.glyphCount;i++)if(f.glyphs[i].value==codepoint){gi=i;break;}if(gi>=0)x+=(f.glyphs[gi].advanceX?f.glyphs[gi].advanceX:f.recs[gi].width)*size/f.baseSize+spacing;}return{std::max(max,x),lines*size};}
const char* TextFormat(const char*f,...){static char b[4][128];static int i;i=(i+1)&3;va_list v;va_start(v,f);std::vsnprintf(b[i],128,f,v);va_end(v);return b[i];}
bool IsKeyDown(int k){int m=0;switch(k){case KEY_LEFT:m=KEY_DLEFT;break;case KEY_RIGHT:m=KEY_DRIGHT;break;case KEY_UP:m=KEY_DUP;break;case KEY_DOWN:m=KEY_DDOWN;break;case KEY_SPACE:m=KEY_B|KEY_X;break;case KEY_W:m=CTR_KEY_A|KEY_Y|KEY_L|CTR_KEY_R;break;case KEY_S:m=KEY_DDOWN;break;case KEY_A:m=KEY_DLEFT;break;case KEY_D:m=KEY_DRIGHT;break;}return (held&m)!=0;}
bool IsKeyPressed(int k){int m=0;switch(k){case KEY_LEFT:m=KEY_DLEFT;break;case KEY_RIGHT:m=KEY_DRIGHT;break;case KEY_UP:m=KEY_DUP;break;case KEY_DOWN:m=KEY_DDOWN;break;case KEY_SPACE:m=KEY_B|KEY_X;break;case KEY_ENTER:m=CTR_KEY_A;break;case KEY_W:m=CTR_KEY_A|KEY_Y|KEY_L|CTR_KEY_R;break;case KEY_S:m=KEY_DDOWN;break;case KEY_R:m=0;break;case KEY_ESCAPE:m=KEY_START;break;}return(down&m)!=0;}
bool IsGamepadButtonDown(int,int b){return(held&keyForButton(b))!=0;} bool IsGamepadButtonPressed(int,int b){return(down&keyForButton(b))!=0;}
float GetGamepadAxisMovement(int,int axis){circlePosition p;hidCircleRead(&p);return std::clamp((axis==GAMEPAD_AXIS_LEFT_Y?-p.dy:p.dx)/156.0f,-1.0f,1.0f);} bool IsMouseButtonPressed(int){return(down&KEY_TOUCH)!=0;}Vector2 GetMousePosition(){touchPosition p;hidTouchRead(&p);return{(float)p.px,(float)p.py};}
int GetRandomValue(int a,int b){return a+std::rand()%(b-a+1);}void TraceLog(int,const char*,...){}void TakeScreenshot(const char*){}void ToggleFullscreen(){}
void InitAudioDevice(){
    if(audioReady)return;
    audioReady=ndspInit()==0;
    if(audioReady)ndspSetOutputMode(NDSP_OUTPUT_STEREO);
}
void CloseAudioDevice(){if(musicPreloadThread)finishMusicPreload(true);discardFinishedMusicPreload();if(!audioReady)return;for(int c=0;c<24;c++)ndspChnReset(c);for(std::size_t i=0;i<audioClips.size();i++)if(audioClips[i]){if(audioClips[i]->data)linearFree(audioClips[i]->data);delete audioClips[i];audioClips[i]=nullptr;}ndspExit();audioReady=false;}
void SetMasterVolume(float v){masterGain=std::clamp(v,0.0f,1.0f);for(int c=0;c<24;c++)if(auto*clip=audioClip(channelClip[c]))setChannelMix(c,clip->volume);}
Sound LoadSound(const char* path){auto*clip=decodeAudio(path,false);if(!clip)return{};audioClips.push_back(clip);return{(unsigned)audioClips.size()};}
void UnloadSound(Sound s){freeAudio(s.id);}bool IsSoundValid(Sound s){return audioClip(s.id)!=nullptr;}
void SetSoundVolume(Sound s,float v){if(auto*clip=audioClip(s.id))clip->volume=std::clamp(v,0.0f,1.0f);}
void PlaySound(Sound s){auto*clip=audioClip(s.id);if(!clip)return;const int channel=nextSfxChannel;nextSfxChannel=nextSfxChannel%23+1;channelClip[channel]=s.id;startChannel(channel,clip,false);}
Music LoadMusicStream(const char* path){
    const std::string wanted=normalize(path);
    AudioClip* clip=nullptr;
    // Room entry must never join an unfinished decoder. The game retries this
    // handoff on following frames while the previous room's music continues.
    if(musicPreloadThread && musicPreloadPath==wanted) {
        if(!musicPreloadDone.load(std::memory_order_acquire))return{};
        finishMusicPreload(false);
    }
    if(!musicPreloadThread && musicPreloadPath==wanted && musicPreloadDone.load(std::memory_order_acquire)) {
        clip=musicPreloadClip;musicPreloadClip=nullptr;musicPreloadPath.clear();
        musicPreloadDone.store(false,std::memory_order_release);
    }
    if(!clip){PreloadMusicStream(path);return{};}
    if(!clip)return{};audioClips.push_back(clip);return{(unsigned)audioClips.size(),true};
}
void PreloadMusicStream(const char* path){
    if(!audioReady||!path||!*path)return;
    const std::string wanted=normalize(path);
    if(musicPreloadPath==wanted && (musicPreloadThread||musicPreloadDone.load(std::memory_order_acquire)))return;
    if(musicPreloadThread) {
        if(!musicPreloadDone.load(std::memory_order_acquire))return;
        finishMusicPreload(false);
    }
    discardFinishedMusicPreload();
    musicPreloadPath=wanted;musicPreloadDone.store(false,std::memory_order_release);
    auto* argument=new std::string(wanted);
    // Decode on the Old 3DS system core allocation so MP3/Vorbis work cannot
    // steal the render budget. If the firmware refuses that worker, decode once
    // synchronously instead of retrying forever and leaving music silent.
    // Prepared PCM avoids codec recursion and the hardware crash caused by decoder stack pressure.
    musicPreloadThread=threadCreate(musicPreloadWorker,argument,64*1024,0x30,musicWorkerCore,false);
    if(!musicPreloadThread) {
        delete argument;
        musicPreloadClip=decodeAudio(wanted.c_str(),true);
        musicPreloadDone.store(true,std::memory_order_release);
    }
}
void UnloadMusicStream(Music m){freeAudio(m.id);}bool IsMusicValid(Music m){return audioClip(m.id)!=nullptr;}
void PlayMusicStream(Music m){auto*clip=audioClip(m.id);if(!clip)return;channelClip[0]=m.id;startChannel(0,clip,m.looping);}
void StopMusicStream(Music m){if(channelClip[0]==m.id){ndspChnReset(0);channelClip[0]=0;}}
void PauseMusicStream(Music m){if(channelClip[0]==m.id)ndspChnSetPaused(0,true);}void ResumeMusicStream(Music m){if(channelClip[0]==m.id)ndspChnSetPaused(0,false);}
void UpdateMusicStream(Music){}void SetMusicVolume(Music m,float v){if(auto*clip=audioClip(m.id)){clip->volume=std::clamp(v,0.0f,1.0f);if(channelClip[0]==m.id)setChannelMix(0,clip->volume);}}
void Draw3DSBottomHud(Texture2D bg,Texture2D choc,Texture2D skull,Texture2D arrow,Texture2D cross,Texture2D longArrow,Texture2D pauseArt,Texture2D soundIcon,Texture2D popup,Texture2D bossbarBack,Texture2D bossbar,Font f,int chocolates,int deaths,int mode,int selection,bool gameCompleted,int language,bool muted,const char* signMessage,bool showBoss,float bossLife,Color bossFill){
    beginTarget(bottom);C2D_TargetClear(bottom,color32(BLACK));bool old=screenDrawing;screenDrawing=false;viewOffsetX=viewOffsetY=0;
    DrawTexturePro(bg,{46,0,291,218},{0,0,320,240},{0,0},0,WHITE);
    const auto anchored=[](Texture2D texture,float x,float y,Vector2 origin) {
        if(!texture.id)return;
        DrawTexturePro(texture,{0,0,(float)texture.width,(float)texture.height},
            {x,y,(float)texture.width,(float)texture.height},origin,0,WHITE);
    };
    const auto centeredLines=[&](const char* message,float centerX,float centerY,float size,Color color) {
        if(!message||!*message)return;
        const float blockHeight=MeasureTextEx(f,message,size,0).y;
        float lineY=centerY-blockHeight/2;
        const char* line=message;
        while(*line) {
            const char* end=std::strchr(line,'\n');
            const std::string value(line,end?(std::size_t)(end-line):std::strlen(line));
            const float width=MeasureTextEx(f,value.c_str(),size,0).x;
            DrawTextEx(f,value.c_str(),{centerX-width/2,lineY},size,0,color);
            lineY+=size;
            if(!end)break;
            line=end+1;
        }
    };
    if(mode==1) {
        // Reuse the exact original title-menu presentation: original font,
        // spr_arrow selection frame and spr_cross locked Game+ marker.
        const char* en[]={"Play","Play+","Credits"};const char* pt[]={"Jogar","Jogar+","Créditos"};
        constexpr float firstY=96.0f,spacing=24.0f;
        for(int i=0;i<3;i++){const char* label=language?pt[i]:en[i];Color color=(i==1&&!gameCompleted)?GRAY:RAYWHITE;float width=MeasureTextEx(f,label,12,0).x;DrawTextEx(f,label,{160-width/2,firstY+i*spacing-6},12,0,color);}
        anchored(arrow,109,firstY+selection*spacing,{0,12});
        if(!gameCompleted)anchored(cross,160,firstY+spacing,{48,9});
    } else if(mode==2) {
        const char* en[]={"Resume","Reset","Menu"};const char* pt[]={"Continuar","Reiniciar","Menu"};
        constexpr float rows[]={72,96,120,168};
        anchored(pauseArt,160,120,{88,88});
        for(int i=0;i<3;i++){const char* label=language?pt[i]:en[i];float width=MeasureTextEx(f,label,12,0).x;DrawTextEx(f,label,{160-width/2,rows[i]-6},12,0,RAYWHITE);}
        anchored(longArrow,97,rows[std::clamp(selection,0,3)],{0,12});
        anchored(soundIcon,160,rows[3],{10,10});
    } else if(signMessage && *signMessage) {
        // Match the original sign presentation, but reserve the upper screen
        // exclusively for gameplay on 3DS.
        anchored(popup,160,120,{126,38});
        centeredLines(signMessage,160,120,8,BLACK);
    } else if(mode==3) {
        const char* credits=language
            ? "Programado por Annie\n\nGráficos por Pavão Gripado e IGustaMe\n\nMúsicas por BainoLOL\n\nObrigado aos que testaram o jogo\npelo feedback e obrigado a você\npor jogar!"
            : "Programmed by Annie\n\nGraphics by Pavão Gripado and IGustaMe\n\nMusic by BainoLOL\n\nThanks to those who tested the game\nfor the feedback and thanks to you\nfor playing!";
        centeredLines(credits,160,120,8,RAYWHITE);
    } else {
        DrawTexturePro(choc,{0,0,(float)choc.width,(float)choc.height},{70,104,24,24},{0,0},0,WHITE);
        DrawTexturePro(skull,{0,0,(float)skull.width,(float)skull.height},{218,104,24,24},{0,0},0,WHITE);
        DrawTextEx(f,TextFormat("%d",chocolates),{100,106},18,0,RAYWHITE);DrawTextEx(f,TextFormat("%d",deaths),{248,106},18,0,RAYWHITE);
    }
    if(showBoss) {
        constexpr float x=25.0f,y=212.0f;
        anchored(bossbarBack,x,y,{0,0});
        DrawRectangle((int)x+18,(int)y+7,(int)std::max(0.0f,bossLife-68.0f),13,bossFill);
        anchored(bossbar,x,y,{0,0});
    }
    screenDrawing=old;
}
