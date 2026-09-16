#include "raylib.h"
#include "asset_map.hpp"
#include <pspkernel.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <psppower.h>
#include <zlib.h>
#include <malloc.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace {
alignas(16) unsigned int displayList[262144];
constexpr unsigned frameBytes=512*272*4;
constexpr unsigned lightOffset=frameBytes*2;
volatile bool quitting=false;
bool initialized=false,listOpen=false,gpuPending=false,screen=false,world=false;
int currentTarget=-1;
int boundPart=-2;
void* drawBuffer=nullptr;
Camera2D camera{};
SceCtrlData pad{};
unsigned held=0,pressed=0;
float offsetX=0,offsetY=0,outputScale=1;
float clipLeft=0,clipTop=0,clipRight=480,clipBottom=272;
unsigned long long serial=0;
FILE* textureFile=nullptr;
FILE* maskFile=nullptr;
std::string dataRoot;
struct CachedPart { void* pixels{}; unsigned long long used{}; };
std::vector<CachedPart> cache(std::size(pspParts));
std::size_t cacheBytes=0;
std::vector<unsigned char> compressedScratch;
enum class Kind {Texture,Rect,Circle,Subtract,Normal};
struct Command {Kind kind;Texture2D tex{};Rectangle src{},dst{};Vector2 origin{};float angle{};Color color{},edge{};bool world{};};
std::vector<Command> commands;
struct Vertex {float u,v;unsigned color;float x,y,z;};
std::vector<Vertex> spriteBatch;
bool batchingSprites=false;
int batchPart=-1;
unsigned rgba(Color c){return c.r|(c.g<<8)|(c.b<<16)|(c.a<<24);}
int exitCallback(int,int,void*){quitting=true;return 0;}
int callbackThread(SceSize,void*){int cb=sceKernelCreateCallback("ElliExit",exitCallback,nullptr);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
void presentPending(){
    if(!gpuPending)return;
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    drawBuffer=sceGuSwapBuffers();
    gpuPending=false;
}
void startList(){if(!listOpen){presentPending();sceGuStart(GU_DIRECT,displayList);listOpen=true;}}
void syncList(){
    if(listOpen){sceGuFinish();sceGuSync(0,0);listOpen=false;}
    else if(gpuPending){sceGuSync(0,0);gpuPending=false;}
}
void submitList(){if(listOpen){sceGuFinish();listOpen=false;gpuPending=true;}}
void normalBlend(){sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);}
void target(int id){
    startList();currentTarget=id;boundPart=-2;
    sceGuDrawBufferList(GU_PSM_8888,id==1?reinterpret_cast<void*>(lightOffset):drawBuffer,512);
    clipLeft=clipTop=0;clipRight=id==1?384:480;clipBottom=id==1?218:272;
    sceGuScissor(0,0,static_cast<int>(clipRight),static_cast<int>(clipBottom));
    sceGuEnable(GU_SCISSOR_TEST);normalBlend();
}
void establishRoot(const char* path){
    if(!dataRoot.empty())return;
    std::string p=path;std::replace(p.begin(),p.end(),'\\','/');
    auto marker=p.find("assets/");if(marker==std::string::npos)marker=p.find("ASSETS/");
    if(marker==std::string::npos)throw std::runtime_error("Asset root missing");
    dataRoot=p.substr(0,marker)+"ASSETS/";
    textureFile=std::fopen((dataRoot+"TEX.BIN").c_str(),"rb");
    maskFile=std::fopen((dataRoot+"MASK.BIN").c_str(),"rb");
    if(!textureFile||!maskFile)throw std::runtime_error("PSP texture pack missing");
}
int assetId(const char* path){
    establishRoot(path);
    static std::unordered_map<std::string,int> map;
    if(map.empty())for(std::size_t i=0;i<std::size(pspAssets);++i)map.emplace(pspAssets[i].path,i);
    std::string p=path;std::replace(p.begin(),p.end(),'\\','/');auto pos=p.find("assets/");
    if(pos!=std::string::npos)p=p.substr(pos);
    auto found=map.find(p);return found==map.end()?-1:found->second;
}
void* partPixels(std::size_t index,bool protectCurrent=true){
    auto& c=cache[index];c.used=serial;
    if(c.pixels)return c.pixels;
    const auto& part=pspParts[index];
    const std::size_t bytes=part.tw*part.th*4;
    // Never release a texture referenced by the current GPU command list.
    constexpr std::size_t cacheLimit=10*1024*1024;
    while(cacheBytes+bytes>cacheLimit){
        std::size_t oldest=cache.size();
        for(std::size_t i=0;i<cache.size();++i)if(cache[i].pixels&&(!protectCurrent||cache[i].used<serial)&&
            (oldest==cache.size()||cache[i].used<cache[oldest].used))oldest=i;
        if(oldest==cache.size())break;
        std::free(cache[oldest].pixels);cache[oldest].pixels=nullptr;
        cacheBytes-=pspParts[oldest].tw*pspParts[oldest].th*4;
    }
    compressedScratch.resize(part.size);
    std::fseek(textureFile,part.offset,SEEK_SET);
    if(std::fread(compressedScratch.data(),1,part.size,textureFile)!=part.size)throw std::runtime_error("Texture pack read failed");
    c.pixels=memalign(16,bytes);if(!c.pixels)throw std::runtime_error("Texture memory exhausted");
    uLongf size=bytes;
    if(uncompress(static_cast<Bytef*>(c.pixels),&size,compressedScratch.data(),part.size)!=Z_OK||size!=bytes)
        throw std::runtime_error("Texture decompression failed");
    sceKernelDcacheWritebackRange(c.pixels,bytes);cacheBytes+=bytes;return c.pixels;
}
Vector2 position(float x,float y){
    if(world){x=(x-camera.target.x)*camera.zoom+camera.offset.x;y=(y-camera.target.y)*camera.zoom+camera.offset.y;}
    return {x*outputScale+offsetX,y*outputScale+offsetY};
}
void flushSpriteBatch(){
    if(spriteBatch.empty())return;
    auto* vertices=static_cast<Vertex*>(sceGuGetMemory(spriteBatch.size()*sizeof(Vertex)));
    std::memcpy(vertices,spriteBatch.data(),spriteBatch.size()*sizeof(Vertex));
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
                   static_cast<int>(spriteBatch.size()),nullptr,vertices);
    spriteBatch.clear();
}
void bindTexturePart(int index,void* pixels){
    if(boundPart==index)return;
    const auto& part=pspParts[index];
    sceGuTexMode(GU_PSM_8888,0,0,0);sceGuTexImage(0,part.tw,part.th,part.tw,pixels);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_NEAREST,GU_NEAREST);
    sceGuTexWrap(GU_CLAMP,GU_CLAMP);sceGuTexScale(1,1);sceGuTexOffset(0,0);sceGuTexFlush();boundPart=index;
}
void textureNow(Texture2D t,Rectangle src,Rectangle dst,Vector2 origin,float angle,Color tint){
    if(!t.id)return;
    if(t.id&0x80000000u){
        if(batchingSprites)flushSpriteBatch();
        boundPart=-2;batchPart=-1;
        sceGuTexSync();sceGuTexFlush();
        sceGuEnable(GU_TEXTURE_2D);sceGuTexMode(GU_PSM_8888,0,0,0);
        sceGuTexImage(0,512,256,512,reinterpret_cast<void*>(0x04000000+lightOffset));
        sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);sceGuTexFilter(GU_NEAREST,GU_NEAREST);sceGuTexWrap(GU_CLAMP,GU_CLAMP);
        auto* v=static_cast<Vertex*>(sceGuGetMemory(2*sizeof(Vertex)));
        v[0]={0,0,rgba(tint),dst.x*outputScale+offsetX,dst.y*outputScale+offsetY,0};
        v[1]={384,218,rgba(tint),(dst.x+dst.width)*outputScale+offsetX,(dst.y+dst.height)*outputScale+offsetY,0};
        sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,nullptr,v);return;
    }
    const auto& asset=pspAssets[t.id-1];
    const float sw=std::abs(src.width),sh=std::abs(src.height);if(sw<=0||sh<=0)return;
    const float zoom=(world?camera.zoom:1)*outputScale;
    const auto anchor=position(dst.x,dst.y);
    const bool fx=src.width<0,fy=src.height<0;
    const bool axisAligned=std::abs(angle)<0.0001F;
    const float cs=axisAligned?1.0F:std::cos(angle*PI/180),sn=axisAligned?0.0F:std::sin(angle*PI/180);
    const unsigned vertexColour=rgba(tint);
    int first=0,last=asset.count;
    if(src.x>=0&&src.y>=0&&sw<=256&&sh<=256){
        const int candidate=int(src.y/256)*((asset.width+255)/256)+int(src.x/256);
        if(candidate>=0&&candidate<asset.count){const auto& p=pspParts[asset.first+candidate];
            if(src.x+sw<=p.x+p.width&&src.y+sh<=p.y+p.height){first=candidate;last=candidate+1;}}
    }
    for(int n=first;n<last;++n){
        const int index=asset.first+n;const auto& part=pspParts[index];
        const float l=std::max(src.x,float(part.x)),top=std::max(src.y,float(part.y));
        const float r=std::min(src.x+sw,float(part.x+part.width)),b=std::min(src.y+sh,float(part.y+part.height));
        if(r<=l||b<=top)continue;
        const float x0=((fx?src.x+sw-r:l-src.x)*dst.width/sw-origin.x)*zoom;
        const float y0=((fy?src.y+sh-b:top-src.y)*dst.height/sh-origin.y)*zoom;
        const float x1=x0+(r-l)*dst.width/sw*zoom,y1=y0+(b-top)*dst.height/sh*zoom;
        Vector2 points[4];
        float minx,maxx,miny,maxy;
        if(axisAligned){
            points[0]={anchor.x+x0,anchor.y+y0};points[1]={anchor.x+x1,anchor.y+y0};
            points[2]={anchor.x+x0,anchor.y+y1};points[3]={anchor.x+x1,anchor.y+y1};
            minx=std::min(points[0].x,points[3].x);maxx=std::max(points[0].x,points[3].x);
            miny=std::min(points[0].y,points[3].y);maxy=std::max(points[0].y,points[3].y);
        }else{
            const float xs[4]={x0,x1,x0,x1},ys[4]={y0,y0,y1,y1};
            minx=miny=1e9f;maxx=maxy=-1e9f;
            for(int j=0;j<4;++j){points[j]={anchor.x+xs[j]*cs-ys[j]*sn,anchor.y+xs[j]*sn+ys[j]*cs};minx=std::min(minx,points[j].x);maxx=std::max(maxx,points[j].x);miny=std::min(miny,points[j].y);maxy=std::max(maxy,points[j].y);}
        }
        if(maxx<=clipLeft||maxy<=clipTop||minx>=clipRight||miny>=clipBottom)continue;
        void* pixels=partPixels(index);
        sceGuEnable(GU_TEXTURE_2D);
        const float u0=(fx?r:l)-part.x,u1=(fx?l:r)-part.x,v0=(fy?b:top)-part.y,v1=(fy?top:b)-part.y;
        if(batchingSprites&&axisAligned&&points[3].x>=points[0].x&&points[3].y>=points[0].y){
            if(batchPart!=index){flushSpriteBatch();bindTexturePart(index,pixels);batchPart=index;}
            // Keep each primitive comfortably below the GE transfer limit.
            // Very large room-tile submissions could otherwise manifest as a
            // transient solid-colour rectangle on real PSP hardware.
            if(spriteBatch.size()>=256)flushSpriteBatch();
            spriteBatch.push_back({u0,v0,vertexColour,points[0].x,points[0].y,0});
            spriteBatch.push_back({u1,v1,vertexColour,points[3].x,points[3].y,0});
        }else if(axisAligned&&points[3].x>=points[0].x&&points[3].y>=points[0].y){
            bindTexturePart(index,pixels);
            auto* v=static_cast<Vertex*>(sceGuGetMemory(2*sizeof(Vertex)));
            v[0]={u0,v0,vertexColour,points[0].x,points[0].y,0};
            v[1]={u1,v1,vertexColour,points[3].x,points[3].y,0};
            sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,nullptr,v);
        }else{
            if(batchingSprites)flushSpriteBatch();
            bindTexturePart(index,pixels);batchPart=index;
            auto* v=static_cast<Vertex*>(sceGuGetMemory(4*sizeof(Vertex)));
            v[0]={u0,v0,vertexColour,points[0].x,points[0].y,0};v[1]={u1,v0,vertexColour,points[1].x,points[1].y,0};
            v[2]={u0,v1,vertexColour,points[2].x,points[2].y,0};v[3]={u1,v1,vertexColour,points[3].x,points[3].y,0};
            sceGuDrawArray(GU_TRIANGLE_STRIP,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,nullptr,v);
        }
    }
}
void rectNow(Rectangle r,Color c){
    if(batchingSprites)flushSpriteBatch();
    auto p=position(r.x,r.y);float z=(world?camera.zoom:1)*outputScale;
    const float x2=p.x+r.width*z,y2=p.y+r.height*z;
    if(std::max(p.x,x2)<=clipLeft||std::max(p.y,y2)<=clipTop||
       std::min(p.x,x2)>=clipRight||std::min(p.y,y2)>=clipBottom)return;
    sceGuDisable(GU_TEXTURE_2D);auto* v=static_cast<Vertex*>(sceGuGetMemory(2*sizeof(Vertex)));
    v[0]={0,0,rgba(c),p.x,p.y,0};v[1]={0,0,rgba(c),x2,y2,0};
    sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,nullptr,v);
}
void circleNow(Rectangle r,Color center,Color edge){
    if(batchingSprites)flushSpriteBatch();
    auto p=position(r.x,r.y);const float radius=r.width*(world?camera.zoom:1)*outputScale;
    if(p.x+radius<=clipLeft||p.y+radius<=clipTop||p.x-radius>=clipRight||p.y-radius>=clipBottom)return;
    // At the native 218-pixel viewport, 32 sides are visually circular while
    // halving the geometry and command-list pressure of every light.
    static const auto unit=[](){std::vector<Vector2> v;v.reserve(33);for(int i=0;i<=32;++i)v.push_back({std::cos(i*2*PI/32),std::sin(i*2*PI/32)});return v;}();
    sceGuDisable(GU_TEXTURE_2D);auto* v=static_cast<Vertex*>(sceGuGetMemory(34*sizeof(Vertex)));
    v[0]={0,0,rgba(center),p.x,p.y,0};
    for(int i=0;i<=32;++i)v[i+1]={0,0,rgba(edge),p.x+unit[i].x*radius,p.y+unit[i].y*radius,0};
    sceGuDrawArray(GU_TRIANGLE_FAN,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,34,nullptr,v);
}
void replay(){
    // The PSP has one display mode: proportional full screen. Filling the
    // physical height leaves less than one pixel of horizontal margin and
    // preserves the original 384x218 aspect ratio without stretching.
    outputScale=272.0f/218.0f;
    offsetX=(480.0f-384.0f*outputScale)*0.5f;
    offsetY=0;
    clipLeft=clipTop=0;clipRight=480;clipBottom=272;
    sceGuScissor(0,0,480,272);
    spriteBatch.clear();batchPart=-1;batchingSprites=true;
    for(const auto& c:commands){world=c.world;switch(c.kind){
        case Kind::Texture:textureNow(c.tex,c.src,c.dst,c.origin,c.angle,c.color);break;
        case Kind::Rect:rectNow(c.dst,c.color);break;
        case Kind::Circle:circleNow(c.dst,c.color,c.edge);break;
        case Kind::Subtract:flushSpriteBatch();sceGuBlendFunc(GU_ADD,GU_FIX,GU_ONE_MINUS_SRC_COLOR,0,0);break;
        case Kind::Normal:flushSpriteBatch();normalBlend();break;
    }}
    flushSpriteBatch();batchingSprites=false;batchPart=-1;
    offsetX=offsetY=0;outputScale=1;world=false;
    clipLeft=clipTop=0;clipRight=480;clipBottom=272;
    sceGuScissor(0,0,480,272);normalBlend();
}
unsigned keyMask(int key){switch(key){
    case KEY_A:case KEY_LEFT:return PSP_CTRL_LEFT;case KEY_D:case KEY_RIGHT:return PSP_CTRL_RIGHT;
    case KEY_S:case KEY_DOWN:return PSP_CTRL_DOWN;case KEY_UP:return PSP_CTRL_UP;
    case KEY_SPACE:return PSP_CTRL_CROSS|PSP_CTRL_SQUARE;
    case KEY_W:return PSP_CTRL_CIRCLE|PSP_CTRL_TRIANGLE|PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER;
    case KEY_ENTER:return PSP_CTRL_CROSS|PSP_CTRL_CIRCLE;
    case KEY_ESCAPE:return PSP_CTRL_START;default:return 0;
}}
struct Clip{std::vector<short> pcm;float gain=1;bool paused=false;};
struct Voice{unsigned clip{};std::size_t cursor{};};
std::vector<Clip*> clips(1,nullptr);Voice voices[24]{};
int sfxVoice=1,audioChannel=-1,audioThread=-1,audioLock=-1;
volatile bool audioRunning=false;float masterGain=1;
alignas(64) short audioBuffer[2][1024*2];
void lockAudio(){if(audioLock>=0)sceKernelWaitSema(audioLock,1,nullptr);}
void unlockAudio(){if(audioLock>=0)sceKernelSignalSema(audioLock,1);}
Clip* clip(unsigned id){return id<clips.size()?clips[id]:nullptr;}
std::string pcmPath(const char* path){establishRoot(path);std::string p=path;auto s=p.find_last_of("/\\");p=p.substr(s==std::string::npos?0:s+1);p=p.substr(0,p.find_last_of('.'));for(auto& c:p)if(c>='a'&&c<='z')c-=32;return dataRoot+p+".PCM";}
int audioWorker(SceSize,void*){
    int slot=0;int mix[2048];
    while(audioRunning){
        std::memset(mix,0,sizeof(mix));lockAudio();
        for(auto& voice:voices){auto* c=clip(voice.clip);if(!c||c->paused)continue;
            // One fixed-point conversion per active voice avoids thousands of
            // floating-point multiplies in every 1024-frame audio block.
            const int gain=static_cast<int>(std::lround(c->gain*masterGain*256.0F));
            const std::size_t count=std::min<std::size_t>(2048,c->pcm.size()-voice.cursor);
            for(std::size_t i=0;i<count;++i)mix[i]+=(int(c->pcm[voice.cursor+i])*gain)>>8;
            voice.cursor+=count;if(voice.cursor>=c->pcm.size())voice.clip=0;
        }
        unlockAudio();for(int i=0;i<2048;++i)audioBuffer[slot][i]=std::clamp(mix[i],-32768,32767);
        sceAudioOutputPannedBlocking(audioChannel,PSP_AUDIO_VOLUME_MAX,PSP_AUDIO_VOLUME_MAX,audioBuffer[slot]);slot^=1;
    }return 0;
}
void releaseClip(unsigned id){lockAudio();auto* c=clip(id);if(c){for(auto& v:voices)if(v.clip==id)v={};delete c;clips[id]=nullptr;}unlockAudio();}
int utf8(const char*& s){unsigned c=(unsigned char)*s++;if(c<128)return c;int n=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;int v=c&((1<<(6-n))-1);while(n--&&*s)v=(v<<6)|((unsigned char)*s++&63);return v;}
}

void SetConfigFlags(unsigned){}void ClearWindowState(unsigned){}
void InitWindow(int,int,const char*){
    if(initialized)return;
    // All retail PSP models supported by this port expose the standard 333 MHz
    // game clock. Request it explicitly instead of inheriting a 222 MHz shell
    // clock from the launcher/CFW.
    scePowerSetClockFrequency(333,333,166);
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    int cb=sceKernelCreateThread("ElliCallbacks",callbackThread,0x11,4096,0,nullptr);if(cb>=0)sceKernelStartThread(cb,0,nullptr);
    sceGuInit();startList();sceGuDrawBuffer(GU_PSM_8888,nullptr,512);
    sceGuDispBuffer(480,272,reinterpret_cast<void*>(frameBytes),512);
    sceGuOffset(2048-240,2048-136);sceGuViewport(2048,2048,480,272);
    sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_CULL_FACE);sceGuDisable(GU_LIGHTING);
    sceGuShadeModel(GU_SMOOTH);sceGuScissor(0,0,480,272);sceGuEnable(GU_SCISSOR_TEST);normalBlend();
    syncList();sceDisplayWaitVblankStart();sceGuDisplay(GU_TRUE);commands.reserve(4096);spriteBatch.reserve(8192);compressedScratch.reserve(256*1024);initialized=true;
}
void CloseWindow(){if(!initialized)return;syncList();sceGuTerm();for(auto& c:cache)std::free(c.pixels);if(textureFile)std::fclose(textureFile);if(maskFile)std::fclose(maskFile);initialized=false;}
bool WindowShouldClose(){unsigned previous=held;sceCtrlPeekBufferPositive(&pad,1);held=pad.Buttons;if(pad.Lx<96)held|=PSP_CTRL_LEFT;if(pad.Lx>160)held|=PSP_CTRL_RIGHT;if(pad.Ly<96)held|=PSP_CTRL_UP;if(pad.Ly>160)held|=PSP_CTRL_DOWN;pressed=held&~previous;return quitting;}
void SetExitKey(int){}void SetTargetFPS(int){}int GetScreenWidth(){return 480;}int GetScreenHeight(){return 272;}
double GetTime(){return sceKernelGetSystemTimeWide()/1000000.0;}
void BeginDrawing(){screen=true;target(-1);world=false;}
void EndDrawing(){submitList();screen=false;currentTarget=-1;++serial;}
void ClearBackground(Color c){if(currentTarget==0&&!screen){commands.clear();return;}startList();sceGuClearColor(rgba(c));sceGuClear(GU_COLOR_BUFFER_BIT);}
void BeginMode2D(Camera2D c){camera=c;world=true;}void EndMode2D(){world=false;}
Vector2 GetWorldToScreen2D(Vector2 p,Camera2D c){return{(p.x-c.target.x)*c.zoom+c.offset.x,(p.y-c.target.y)*c.zoom+c.offset.y};}
RenderTexture2D LoadRenderTexture(int w,int h){static unsigned next=0;unsigned id=0x80000000u|++next;return{id,{id,w,h,1,0},{}};}
void UnloadRenderTexture(RenderTexture2D){}
void BeginTextureMode(RenderTexture2D t){currentTarget=int(t.id&0x7fffffffu)-1;if(currentTarget==1)target(1);}
void EndTextureMode(){currentTarget=-1;world=false;}
Texture2D LoadTexture(const char* p){int i=assetId(p);return i<0?Texture2D{}:Texture2D{unsigned(i+1),pspAssets[i].width,pspAssets[i].height,1,0};}
void PreloadTexture(Texture2D texture){
    if(!texture.id||(texture.id&0x80000000u))return;
    const auto& asset=pspAssets[texture.id-1];
    for(int part=0;part<asset.count;++part)(void)partPixels(static_cast<std::size_t>(asset.first+part),false);
}
void UnloadTexture(Texture2D){}void SetTextureFilter(Texture2D,int){}
Image LoadImage(const char* p){int i=assetId(p);if(i<0)return{};auto& a=pspAssets[i];std::vector<unsigned char> compressed(a.maskSize),alpha(a.width*a.height);std::fseek(maskFile,a.maskOffset,SEEK_SET);std::fread(compressed.data(),1,a.maskSize,maskFile);uLongf count=alpha.size();if(uncompress(alpha.data(),&count,compressed.data(),compressed.size())!=Z_OK)return{};auto* colors=static_cast<Color*>(std::malloc(count*sizeof(Color)));if(!colors)return{};for(unsigned n=0;n<count;++n)colors[n]={255,255,255,alpha[n]};return{colors,a.width,a.height,1,0};}
Color* LoadImageColors(Image i){auto* c=static_cast<Color*>(std::malloc(i.width*i.height*sizeof(Color)));if(c&&i.data)std::memcpy(c,i.data,i.width*i.height*sizeof(Color));return c;}
void UnloadImageColors(Color* c){std::free(c);}void UnloadImage(Image i){std::free(i.data);}
void DrawTexturePro(Texture2D t,Rectangle s,Rectangle d,Vector2 o,float r,Color c){if(currentTarget==0&&!screen){commands.push_back({Kind::Texture,t,s,d,o,r,c,{},world});return;}if(screen&&t.id==0x80000001u){replay();return;}textureNow(t,s,d,o,r,c);}
void DrawTextureRec(Texture2D t,Rectangle s,Vector2 p,Color c){DrawTexturePro(t,s,{p.x,p.y,std::abs(s.width),std::abs(s.height)},{},0,c);}
void DrawRectangle(int x,int y,int w,int h,Color c){Rectangle r{float(x),float(y),float(w),float(h)};if(currentTarget==0&&!screen){commands.push_back({Kind::Rect,{},{},r,{},0,c,{},world});return;}rectNow(r,c);}
void DrawRectangleLinesEx(Rectangle r,float t,Color c){DrawRectangle(r.x,r.y,r.width,t,c);DrawRectangle(r.x,r.y+r.height-t,r.width,t,c);DrawRectangle(r.x,r.y,t,r.height,c);DrawRectangle(r.x+r.width-t,r.y,t,r.height,c);}
void DrawCircleGradient(int x,int y,float radius,Color a,Color b){Rectangle r{float(x),float(y),radius,0};if(currentTarget==0&&!screen){commands.push_back({Kind::Circle,{},{},r,{},0,a,b,world});return;}circleNow(r,a,b);}
bool CheckCollisionRecs(Rectangle a,Rectangle b){return a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y;}
void BeginBlendMode(int){if(currentTarget==0&&!screen){commands.push_back({Kind::Subtract});return;}sceGuBlendFunc(GU_ADD,GU_FIX,GU_ONE_MINUS_SRC_COLOR,0,0);}
void EndBlendMode(){if(currentTarget==0&&!screen){commands.push_back({Kind::Normal});return;}normalBlend();}
Color Fade(Color c,float a){c.a=std::clamp(a,0.0f,1.0f)*c.a;return c;}
void* MemAlloc(unsigned n){return std::calloc(1,n);}void UnloadFont(Font f){std::free(f.recs);std::free(f.glyphs);}
void DrawText(const char*,int,int,int,Color){}int MeasureText(const char* s,int size){return std::strlen(s)*size/2;}
void DrawTextEx(Font f,const char* s,Vector2 p,float size,float spacing,Color c){float x=p.x,y=p.y,scale=size/f.baseSize;while(s&&*s){int code=utf8(s);if(code=='\n'){x=p.x;y+=size;continue;}for(int i=0;i<f.glyphCount;++i)if(f.glyphs[i].value==code){auto r=f.recs[i];auto g=f.glyphs[i];DrawTexturePro(f.texture,r,{x+g.offsetX*scale,y+g.offsetY*scale,r.width*scale,r.height*scale},{},0,c);x+=(g.advanceX?g.advanceX:r.width)*scale+spacing;break;}}}
Vector2 MeasureTextEx(Font f,const char* s,float size,float spacing){float x=0,w=0;int rows=1;while(s&&*s){int code=utf8(s);if(code=='\n'){w=std::max(w,x);x=0;++rows;continue;}for(int i=0;i<f.glyphCount;++i)if(f.glyphs[i].value==code){x+=(f.glyphs[i].advanceX?f.glyphs[i].advanceX:f.recs[i].width)*size/f.baseSize+spacing;break;}}return{std::max(w,x),rows*size};}
const char* TextFormat(const char* fmt,...){static char buffers[4][256];static int slot=0;slot=(slot+1)%4;va_list args;va_start(args,fmt);std::vsnprintf(buffers[slot],256,fmt,args);va_end(args);return buffers[slot];}
bool IsKeyDown(int k){return held&keyMask(k);}bool IsKeyPressed(int k){return pressed&keyMask(k);}
bool IsGamepadButtonDown(int,int){return false;}bool IsGamepadButtonPressed(int,int){return false;}
float GetGamepadAxisMovement(int,int axis){return std::clamp(((axis==GAMEPAD_AXIS_LEFT_Y?pad.Ly:pad.Lx)-128)/127.0f,-1.0f,1.0f);}
bool IsMouseButtonPressed(int){return false;}Vector2 GetMousePosition(){return{-1000,-1000};}
int GetRandomValue(int a,int b){return a+std::rand()%(b-a+1);}void ToggleFullscreen(){}bool IsWindowFullscreen(){return true;}void TakeScreenshot(const char*){}
void TraceLog(int,const char* fmt,...){va_list args;va_start(args,fmt);std::vprintf(fmt,args);std::printf("\n");va_end(args);}
void InitAudioDevice(){audioChannel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,1024,PSP_AUDIO_FORMAT_STEREO);audioLock=sceKernelCreateSema("ElliAudioLock",0,1,1,nullptr);audioRunning=audioChannel>=0;audioThread=sceKernelCreateThread("ElliAudio",audioWorker,0x12,64*1024,PSP_THREAD_ATTR_USER,nullptr);if(audioRunning&&audioThread>=0)sceKernelStartThread(audioThread,0,nullptr);}
void CloseAudioDevice(){audioRunning=false;if(audioThread>=0){sceKernelWaitThreadEnd(audioThread,nullptr);sceKernelDeleteThread(audioThread);}if(audioChannel>=0)sceAudioChRelease(audioChannel);for(std::size_t i=1;i<clips.size();++i)releaseClip(i);if(audioLock>=0)sceKernelDeleteSema(audioLock);audioLock=-1;}
void SetMasterVolume(float gain){lockAudio();masterGain=gain;unlockAudio();}
Sound LoadSound(const char* p){auto path=pcmPath(p);FILE* f=std::fopen(path.c_str(),"rb");if(!f)return{};std::fseek(f,0,SEEK_END);long bytes=std::ftell(f);std::rewind(f);auto* c=new Clip;c->pcm.resize(bytes/2);std::fread(c->pcm.data(),1,bytes,f);std::fclose(f);lockAudio();clips.push_back(c);unsigned id=clips.size()-1;unlockAudio();return{id};}
void UnloadSound(Sound s){releaseClip(s.id);}bool IsSoundValid(Sound s){return clip(s.id);}
void SetSoundVolume(Sound s,float gain){lockAudio();if(auto* c=clip(s.id))c->gain=gain;unlockAudio();}
void PlaySound(Sound s){lockAudio();voices[sfxVoice]={s.id,0};sfxVoice=sfxVoice%23+1;unlockAudio();}
Music LoadMusicStream(const char*){return{};}void UnloadMusicStream(Music){}bool IsMusicValid(Music){return false;}
void PlayMusicStream(Music){}void StopMusicStream(Music){}void PauseMusicStream(Music){}void ResumeMusicStream(Music){}
void UpdateMusicStream(Music){}void SetMusicVolume(Music,float){}
