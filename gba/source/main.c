#include <stdint.h>
#include "assets.h"

#define REG16(a) (*(volatile uint16_t*)(a))
#define REG32(a) (*(volatile uint32_t*)(a))
#define REG_DISPCNT REG16(0x04000000)
#define REG_VCOUNT  REG16(0x04000006)
#define REG_BG2PA   REG16(0x04000020)
#define REG_BG2PB   REG16(0x04000022)
#define REG_BG2PC   REG16(0x04000024)
#define REG_BG2PD   REG16(0x04000026)
#define REG_BG2X    REG32(0x04000028)
#define REG_BG2Y    REG32(0x0400002C)
#define REG_BLDCNT  REG16(0x04000050)
#define REG_BLDALPHA REG16(0x04000052)
#define REG_KEYINPUT REG16(0x04000130)
#define REG_WAITCNT REG16(0x04000204)
#define REG_SOUNDCNT_H REG16(0x04000082)
#define REG_SOUNDCNT_X REG16(0x04000084)
#define REG_SOUNDBIAS REG16(0x04000088)
#define FIFO_A REG32(0x040000A0)
#define FIFO_B REG32(0x040000A4)
#define DMA1SAD REG32(0x040000BC)
#define DMA1DAD REG32(0x040000C0)
#define DMA1CNT REG32(0x040000C4)
#define DMA2SAD REG32(0x040000C8)
#define DMA2DAD REG32(0x040000CC)
#define DMA2CNT REG32(0x040000D0)
#define DMA3SAD REG32(0x040000D4)
#define DMA3DAD REG32(0x040000D8)
#define DMA3CNT REG32(0x040000DC)
#define BG_PALETTE ((volatile uint16_t*)0x05000000)
#define OBJ_PALETTE ((volatile uint16_t*)0x05000200)
#define OAM ((volatile uint16_t*)0x07000000)
#define OBJ_VRAM ((volatile uint16_t*)0x06014000)
#define KEY_A 1
#define KEY_B 2
#define KEY_SELECT 4
#define KEY_START 8
#define KEY_RIGHT 16
#define KEY_LEFT 32
#define KEY_UP 64
#define KEY_DOWN 128
#define KEY_R 256
#define DMA_ENABLE 0x80000000u
#define DMA_32 0x04000000u
#define REG_TM0D REG16(0x04000100)
#define REG_TM0CNT REG16(0x04000102)
#define IWRAM_CODE __attribute__((section(".iwram")))
#define ROM_CODE __attribute__((section(".text"),noinline))

typedef struct {
    int32_t x,y,vx,vy;
    int32_t previous_x,previous_y;
    int16_t camera_x,camera_y,previous_camera_x,previous_camera_y;
    uint16_t keys,pressed_latch;
    uint8_t room,on_ground,facing,anim_tick,frame,group,dash,sign,jump_buffer,coyote,dead,death_timer;
    uint8_t have_slide,have_dash,have_blink,sliding,low_profile,dash_spent,blink_left;
    uint8_t blinking;
    uint8_t chocolates,room_start_chocolates,deaths;
    uint16_t sfx_ticks_a,sfx_ticks_b;
    uint8_t blink_sfx_cooldown,slope_anim_grace,blink_effect_timer,paint_variants_loaded;
    uint8_t wall_lock,wall_coyote;
    int8_t wall_push,wall_coyote_side;
    uint8_t title_timer;
    uint8_t in_menu,paused,pause_selection,ending,true_ending;
    uint16_t ending_timer;
    uint32_t pickup_collected;
} State;
static State g;
static uint8_t displayed_page;
typedef struct {
    EnemyAsset a;
    int32_t previous_x,previous_y;
    int16_t aim_x,aim_y;
    int8_t tilt;
    uint8_t frame,anim_tick,active,spawn_toggle,tilt_back;
} RuntimeEnemy;
static RuntimeEnemy enemies[ENEMY_MAX];
static uint8_t enemy_count;
typedef struct { int32_t x,y; uint8_t age,facing; } Trail;
static Trail trails[10];
static uint8_t trail_effect_frame;
static uint8_t loaded_trail_frame=255;
static uint8_t render_alpha;
static int16_t render_camera_x,render_camera_y;

static IWRAM_CODE void audio_play_sfx(uint8_t id) {
    const AudioAsset* effect=&sfx_assets[id];
    uint16_t ticks=(uint16_t)((effect->length*45u+16383u)/16384u);
    if(!g.sfx_ticks_a || (g.sfx_ticks_b && g.sfx_ticks_a<=g.sfx_ticks_b)) {
        DMA1CNT=0;REG_SOUNDCNT_H|=0x0800;
        DMA1SAD=(uint32_t)(sfx_data+effect->offset);DMA1DAD=(uint32_t)&FIFO_A;DMA1CNT=0xB6400000u;g.sfx_ticks_a=ticks;
    } else {
        DMA2CNT=0;REG_SOUNDCNT_H|=0x8000;
        DMA2SAD=(uint32_t)(sfx_data+effect->offset);DMA2DAD=(uint32_t)&FIFO_B;DMA2CNT=0xB6400000u;g.sfx_ticks_b=ticks;
    }
}
static void audio_init(void) {
    REG_SOUNDCNT_X=0x0080; REG_SOUNDBIAS=0x0200; REG_SOUNDCNT_H=0xBB0C;
    REG_TM0D=0xFC00; REG_TM0CNT=0x0080;
}
static void add_player_trail(uint8_t kind) {
    unsigned slot=0;
    for(unsigned i=0;i<10;i++)if(!trails[i].age){slot=i;break;}else if(trails[i].age<trails[slot].age)slot=i;
    trails[slot].x=g.x;trails[slot].y=g.y;trails[slot].facing=g.facing;
    trails[slot].age=kind?4:9;
    unsigned group=kind?6:7;
    unsigned local=g.group==group?(unsigned)(g.frame-group_start[group]):0;
    trail_effect_frame=(uint8_t)(kind*4+(local&3));
}

static void dma16(const void* src, volatile void* dst, uint32_t halfwords) {
    DMA3SAD=(uint32_t)src; DMA3DAD=(uint32_t)dst; DMA3CNT=DMA_ENABLE|halfwords;
}
static void load_title_tiles(void) {
    if(g.room<ROOM_COUNT-1) {
        const TitleAsset* title=&title_assets[g.room];
        dma16(title_tiles+title->offset,OBJ_VRAM+1536,title->length>>1);
    }
}
static void vblank(void) {
    while(REG_VCOUNT>=160) {}
    while(REG_VCOUNT<160) {}
}
static int bit_at(const unsigned char* data, uint32_t offset, const RoomAsset* r, int x, int y) {
    /* GameMaker rooms do not implicitly collide at their bounds; several exit
       triggers intentionally sit just outside the nominal room rectangle. */
    if(x<0 || y<0 || x>=r->w || y>=r->h) return 0;
    uint32_t n=(uint32_t)y*r->w+(uint32_t)x;
    return (data[offset+(n>>3)]>>(n&7))&1;
}
static int solid_at(const RoomAsset* r,int x,int y) { return bit_at(collision_data,r->collision,r,x,y); }
static int platform_at(const RoomAsset* r,int x,int y) { return bit_at(platform_data,r->platform,r,x,y); }
static int hazard_at(const RoomAsset* r,int x,int y) { return bit_at(hazard_data,r->hazard,r,x,y); }
static int slope_at(const RoomAsset* r,int x,int y) { return r->slope!=0xffffffffu&&bit_at(slope_data,r->slope,r,x,y); }
static IWRAM_CODE int player_hits_shape(const RoomAsset* r,int x,int y,int low_profile) {
    /* The original crouch/slide sprites use bbox y=10..25 around origin y=13.
       Keep their exact shorter mask so Elli can pass below low ceilings. */
    int l=x-7, rr=x+7, t=low_profile?y-3:y-12, b=y+12;
    for(int px=l;px<=rr;px+=7) if(solid_at(r,px,t)||solid_at(r,px,b)) return 1;
    for(int py=t;py<=b;py+=6) if(solid_at(r,l,py)||solid_at(r,rr,py)) return 1;
    return 0;
}
static int player_hits(const RoomAsset* r,int x,int y) {
    return player_hits_shape(r,x,y,g.low_profile);
}
static IWRAM_CODE int player_hits_hazard(const RoomAsset* r,int x,int y) {
    int l=x-7, rr=x+7, t=g.low_profile?y-3:y-12, b=y+12;
    for(int py=t;py<=b;py+=4) for(int px=l;px<=rr;px+=4) if(hazard_at(r,px,py)) return 1;
    return hazard_at(r,rr,b);
}
static int descending_slope_support(const RoomAsset* r,int x,int y,int32_t vx) {
    /* Use the dedicated obj_slope mask: gaps between its diagonal collision
       pixels must not switch the run animation to fall for a single tick. */
    int lead=x+(vx<0?-6:6);
    for(int depth=0;depth<=40;depth+=2)
        if(slope_at(r,lead,y+12+depth)||slope_at(r,x,y+12+depth)) return 1;
    return 0;
}
static int iabs(int n) { return n<0?-n:n; }
static uint16_t isqrt32(uint32_t value) {
    uint32_t root=0,bit=1u<<30;
    while(bit>value)bit>>=2;
    while(bit) {
        if(value>=root+bit){value-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return (uint16_t)root;
}
static int enemy_near(const RuntimeEnemy* e,int x,int y) {
    int dx=(e->a.x>>8)-x,dy=(e->a.y>>8)-y;
    return (uint32_t)(dx*dx+dy*dy)<(uint32_t)e->a.range*e->a.range;
}
static void enemy_aim(RuntimeEnemy* e,int x,int y) {
    int dx=x-(e->a.x>>8),dy=y-(e->a.y>>8);uint16_t d=isqrt32((uint32_t)(dx*dx+dy*dy));
    if(!d)d=1;
    e->aim_x=(int16_t)(dx*e->a.speed/d); e->aim_y=(int16_t)(dy*e->a.speed/d);
}
static void spawn_from(RuntimeEnemy* parent) {
    if(enemy_count>=ENEMY_MAX || parent->a.spawn_sprite==0xffff) return;
    RuntimeEnemy* e=&enemies[enemy_count++];
    uint16_t sprite=parent->a.spawn_sprite; int16_t speed=parent->a.spawn_speed;
    if(parent->a.alt_sprite!=0xffff && (parent->spawn_toggle++&1)) { sprite=parent->a.alt_sprite; speed=parent->a.alt_spawn_speed; }
    e->a=parent->a; e->a.sprite=sprite; e->a.spawn_sprite=e->a.alt_sprite=0xffff;
    e->a.behavior=5; e->a.state=(parent->a.spawn_rate==75)?1:0; e->a.timer=0; e->a.speed=speed;
    e->a.range=0; e->a.scale_x=e->a.scale_y=(parent->a.spawn_rate==75)?128:256;
    e->a.flags=0;e->a.anim_period=(parent->a.spawn_rate==75)?3:0;e->a.pad=0;
    e->previous_x=e->a.x;e->previous_y=e->a.y;e->tilt=0;e->tilt_back=0;
    e->frame=e->anim_tick=e->spawn_toggle=0; e->active=1; enemy_aim(e,g.x>>8,g.y>>8);
}
static void spawn_boss_enemy(uint16_t sprite,uint8_t behavior,int x,int y,int16_t speed,int scale) {
    if(enemy_count>=ENEMY_MAX)return;
    RuntimeEnemy* e=&enemies[enemy_count++];
    e->a.x=(int32_t)x<<8;e->a.y=(int32_t)y<<8;e->a.sprite=sprite;e->a.spawn_sprite=e->a.alt_sprite=0xffff;
    e->a.behavior=behavior;e->a.state=behavior==5?0:1;e->a.timer=0;e->a.direction=0;e->a.range=0;
    e->a.speed=speed;e->a.vertical_speed=0;e->a.spawn_rate=0;e->a.spawn_speed=e->a.alt_spawn_speed=0;
    e->a.scale_x=e->a.scale_y=(int16_t)scale;e->a.harmful=1;e->a.flags=0;e->a.anim_period=5;e->a.pad=0;
    e->previous_x=e->a.x;e->previous_y=e->a.y;e->tilt=0;e->tilt_back=0;
    e->frame=e->anim_tick=e->spawn_toggle=0;e->active=1;enemy_aim(e,g.x>>8,g.y>>8);
}
static int enemy_pixel_at(const RuntimeEnemy* e,int wx,int wy) {
    const EnemySpriteAsset* s=&enemy_sprite_assets[e->a.sprite];
    int ax=iabs(e->a.scale_x),ay=iabs(e->a.scale_y);
    int left=(e->a.x>>8)-((e->a.scale_x<0?s->w-s->ox:s->ox)*ax>>8);
    int top=(e->a.y>>8)-((e->a.scale_y<0?s->h-s->oy:s->oy)*ay>>8);
    int dw=(s->w*ax)>>8,dh=(s->h*ay)>>8;
    int px=wx-left,py=wy-top;
    if(px<0||py<0||px>=dw||py>=dh||!dw||!dh)return 0;
    int sx=px*256/ax,sy=py*256/ay;
    if(e->a.scale_x<0)sx=s->w-1-sx;
    if(e->a.scale_y<0)sy=s->h-1-sy;
    return enemy_pixels[s->offset+(uint32_t)(e->frame%s->frames)*s->w*s->h+(uint32_t)sy*s->w+sx]!=0;
}
static int player_hits_enemy(int x,int y) {
    int player_top=g.low_profile?y-3:y-12;
    for(unsigned n=0;n<enemy_count;n++) {
        RuntimeEnemy* e=&enemies[n]; if(!e->active||!e->a.harmful)continue;
        const EnemySpriteAsset* s=&enemy_sprite_assets[e->a.sprite];
        int ax=iabs(e->a.scale_x),ay=iabs(e->a.scale_y);
        int left=(e->a.x>>8)-((e->a.scale_x<0?s->w-s->ox:s->ox)*ax>>8);
        int top=(e->a.y>>8)-((e->a.scale_y<0?s->h-s->oy:s->oy)*ay>>8);
        int right=left+((s->w*ax)>>8),bottom=top+((s->h*ay)>>8);
        if(x+7<left||x-7>=right||y+12<top||player_top>=bottom)continue;
        for(int py=player_top;py<=y+12;py+=4) for(int px=x-7;px<=x+7;px+=4)
            if(enemy_pixel_at(e,px,py))return 1;
    }
    return 0;
}
static IWRAM_CODE int enemy_hits_solid_at(const RuntimeEnemy* e,int32_t dx,int32_t dy) {
    const EnemySpriteAsset* s=&enemy_sprite_assets[e->a.sprite];
    int ax=iabs(e->a.scale_x),ay=iabs(e->a.scale_y);
    int x=((e->a.x+dx)>>8)-((e->a.scale_x<0?s->w-s->ox:s->ox)*ax>>8);
    int y=((e->a.y+dy)>>8)-((e->a.scale_y<0?s->h-s->oy:s->oy)*ay>>8);
    int w=(s->w*ax)>>8,h=(s->h*ay)>>8; const RoomAsset* r=&room_assets[g.room];
    for(int px=x;px<=x+w;px+=4)if(solid_at(r,px,y)||solid_at(r,px,y+h))return 1;
    for(int py=y;py<=y+h;py+=4)if(solid_at(r,x,py)||solid_at(r,x+w,py))return 1;
    return 0;
}
static IWRAM_CODE void update_enemies(void) {
    int px=g.x>>8,py=g.y>>8;
    for(unsigned n=0;n<enemy_count;n++) {
        RuntimeEnemy* e=&enemies[n]; if(!e->active)continue;
        switch(e->a.behavior) {
        case 1: case 2:
            if(e->a.state==0&&enemy_near(e,px,py)) { e->a.direction=e->a.behavior==2?256:(g.vx>0?-256:g.vx<0?256:0); e->a.state=1; e->frame=1; }
            if(e->a.state==1)e->a.x+=(int32_t)e->a.direction*e->a.speed/256;
            break;
        case 3:
            if(e->a.state==0&&enemy_near(e,px,py)){e->a.state=1;e->frame=1;}
            if(e->a.state==1) { if(enemy_hits_solid_at(e,0,e->a.vertical_speed))e->active=0; else e->a.y+=e->a.vertical_speed; }
            break;
        case 4:
            if(e->a.state==0&&enemy_near(e,px,py))e->a.state=1;
            if(e->a.state==1||e->a.state==2) {
                if(!e->tilt_back){if(++e->tilt>=9)e->tilt_back=1;}
                else if(--e->tilt<=-9)e->tilt_back=0;
            }
            if(e->a.state==1&&++e->a.timer>60){e->a.state=2;e->a.timer=0;e->frame=1;}
            if(e->a.state==2) {
                if(!enemy_near(e,px,py)){e->a.state=0;e->frame=0;}
                else { e->a.speed=512; enemy_aim(e,px,py); e->a.x+=e->aim_x; e->a.y+=e->aim_y; }
            }
            break;
        case 5: case 6:
            if(e->a.behavior==6&&e->a.state==-1&&enemy_near(e,px,py))e->a.state=0;
            if(e->a.state==0) {
                enemy_aim(e,px,py); e->frame=1;
                if(++e->a.timer<40)e->a.y-=256;
                else if(e->a.timer>50){e->a.state=1;e->a.timer=0;}
            } else if(e->a.state==1) {
                if(e->a.scale_x>0&&e->a.scale_x<256){e->a.scale_x+=26;e->a.scale_y+=26;}
                if(e->a.flags&&enemy_hits_solid_at(e,e->aim_x,e->aim_y)){e->aim_x=-e->aim_x;e->aim_y=-e->aim_y;}
                else {e->a.x+=e->aim_x;e->a.y+=e->aim_y;}
            }
            break;
        case 7:
            if(enemy_near(e,px,py)) { e->frame=1; if(++e->a.timer>e->a.spawn_rate){e->a.timer=0;spawn_from(e);} }
            else {e->frame=0;++e->a.timer;}
            break;
        case 8:
            e->a.direction-=5;if(e->a.direction<0)e->a.direction+=360;
            e->a.x+=e->a.speed;if(!enemy_hits_solid_at(e,0,512))e->a.y+=512;
            break;
        case 9: case 10:
            if((e->a.behavior==9&&e->a.timer++==0)||(e->a.behavior==10&&++e->a.timer==16))enemy_aim(e,px,py);
            if(e->a.behavior==9||e->a.timer>=16){e->a.x+=e->aim_x;e->a.y+=e->aim_y;}
            break;
        case 12: {
            if(e->a.state==-2) {
                const EnemySpriteAsset* death=&enemy_sprite_assets[e->a.sprite];
                if(++e->anim_tick>=5){e->anim_tick=0;if(e->frame+1<death->frames)++e->frame;}
                continue;
            }
            ++e->a.timer; ++e->a.direction;
            if(e->a.state==1&&e->a.direction==1)
                spawn_boss_enemy(boss_sprite_ids[4],8,96,165,1024,256);
            if(e->a.state==2&&e->a.direction==76) {
                spawn_boss_enemy(boss_sprite_ids[5],9,295,50,3328,256);
                spawn_boss_enemy(boss_sprite_ids[6],10,250,50,2560,256);
                e->a.sprite=boss_sprite_ids[2];e->frame=e->anim_tick=0;
            }
            /* The GBA build omits the car attack completely. State 3 remains
               as the original recovery window before the cycle restarts. */
            int timing=e->a.state==0?60:e->a.state==1?60:e->a.state==2?120:120;
            if(e->a.timer>timing) {
                int old=e->a.state;e->a.state=(int8_t)((e->a.state+1)&3);e->a.timer=0;e->a.direction=0;
                if(old==1||old==2)--e->a.vertical_speed;
                if(old!=0)audio_play_sfx((old&1)?4:5);
                e->a.sprite=(e->a.state==2)?boss_sprite_ids[1]:boss_sprite_ids[0];e->frame=e->anim_tick=0;
                if(e->a.vertical_speed<=0){e->a.state=-2;e->a.sprite=boss_sprite_ids[3];e->a.anim_period=0;e->frame=e->anim_tick=0;}
            }
            break;
        }
        }
        const EnemySpriteAsset* s=&enemy_sprite_assets[e->a.sprite];
        if(e->a.anim_period&&++e->anim_tick>=e->a.anim_period){e->anim_tick=0;e->frame=(uint8_t)((e->frame+1)%s->frames);}
        /* Active states select the outlined subimage every tick, matching the
           explicit image_index assignments in the original GML. */
        if(((e->a.behavior==1||e->a.behavior==2||e->a.behavior==3)&&e->a.state==1)||
           (e->a.behavior==4&&e->a.state==2)||(e->a.behavior==6&&e->a.state>=0))e->frame=1;
        int x=e->a.x>>8,y=e->a.y>>8;
        if(x<-100||x>room_assets[g.room].w+100||y<-64||y>room_assets[g.room].h+64)e->active=0;
    }
}
static void move_x(const RoomAsset* r,int32_t amount) {
    int32_t target=g.x+amount; int from=g.x>>8, to=target>>8, dir=to<from?-1:1;
    int rise_limit=iabs(to-from);
    if(rise_limit&&player_hits(r,to,g.y>>8)) {
        int y=g.y>>8,rise=0;
        while(rise<=rise_limit&&player_hits(r,to,y-rise))++rise;
        if(rise<=rise_limit) {
            g.y=(y-rise)<<8;
            g.x=target;
            return;
        }
    }
    while(from!=to) {
        if(player_hits(r,from+dir,g.y>>8)) { g.vx=0; g.x=from<<8; return; }
        from+=dir;
    }
    g.x=target;
}
static void move_y(const RoomAsset* r,int32_t amount) {
    int32_t target=g.y+amount; int from=g.y>>8, to=target>>8, dir=to<from?-1:1;
    while(from!=to) {
        int nx=g.x>>8, ny=from+dir;
        int blocked=player_hits(r,nx,ny);
        if(dir>0 && !blocked && !((g.keys&KEY_DOWN)!=0)) {
            int foot=ny+12, oldfoot=from+12;
            if(oldfoot<foot && (platform_at(r,nx-6,foot)||platform_at(r,nx+6,foot))) blocked=1;
        }
        if(blocked) { g.vy=0; g.y=from<<8; return; }
        from+=dir;
    }
    g.y=target;
}
static void load_room(uint8_t id,int restart) {
    if(id>=ROOM_COUNT) id=0;
    if(restart) g.chocolates=g.room_start_chocolates;
    else g.room_start_chocolates=g.chocolates;
    g.room=id; const RoomAsset* r=&room_assets[id];
    g.x=(int32_t)r->start_x<<8; g.y=(int32_t)r->start_y<<8;g.previous_x=g.x;g.previous_y=g.y;
    g.vx=g.vy=0; g.camera_x=g.camera_y=g.previous_camera_x=g.previous_camera_y=0; g.frame=group_start[0]; g.group=0;
    g.jump_buffer=g.coyote=0; g.pressed_latch=0; g.dead=g.death_timer=0;g.paused=g.ending=0;
    g.wall_lock=0;g.wall_push=0;g.wall_coyote=0;g.wall_coyote_side=0;g.slope_anim_grace=0;g.blink_effect_timer=0;g.paint_variants_loaded=0;loaded_trail_frame=255;
    g.title_timer=(!restart&&id<ROOM_COUNT-1)?50:0;
    g.pickup_collected=0; g.sliding=0; g.low_profile=0; g.dash_spent=0; g.blinking=0; g.blink_left=(id==ROOM_COUNT-1)?15:30;
    for(unsigned i=0;i<10;i++)trails[i].age=0;
    enemy_count=(uint8_t)(r->enemy_count>ENEMY_MAX?ENEMY_MAX:r->enemy_count);
    for(unsigned i=0;i<enemy_count;i++) { enemies[i].a=enemy_assets[r->enemy_first+i]; enemies[i].previous_x=enemies[i].a.x;enemies[i].previous_y=enemies[i].a.y;enemies[i].frame=0; enemies[i].anim_tick=0; enemies[i].active=1; enemies[i].spawn_toggle=0; enemies[i].aim_x=enemies[i].aim_y=0;enemies[i].tilt=0;enemies[i].tilt_back=0; }
    dma16(palette_data+r->palette,BG_PALETTE,256);
    if(id==ROOM_COUNT-1) {
        dma16(bossbar_tiles,OBJ_VRAM+1536,BOSSBAR_TILE_HALFWORDS);
        dma16(bossbar_palette,OBJ_PALETTE+112,16);
    }
    load_title_tiles();
}
static void enter_menu(void) {
    g.in_menu=1;g.paused=g.ending=0;g.pressed_latch=0;
    for(unsigned i=0;i<10;i++)trails[i].age=0;
    dma16(menu_palette,BG_PALETTE,256);
}
static void enter_pause(void) {
    g.paused=1;g.pause_selection=0;
    dma16(pause_palette,BG_PALETTE,256);
}
static void leave_pause(void) {
    g.paused=0;
    dma16(palette_data+room_assets[g.room].palette,BG_PALETTE,256);
}
static void enter_ending(void) {
    g.ending=1;g.ending_timer=0;g.true_ending=g.chocolates>14;
    dma16(end_palette,BG_PALETTE,256);
}
static void update(void) {
    const RoomAsset* r=&room_assets[g.room];
    if(g.sfx_ticks_a && --g.sfx_ticks_a==0) { DMA1CNT=0;REG_SOUNDCNT_H|=0x0800; }
    if(g.sfx_ticks_b && --g.sfx_ticks_b==0) { DMA2CNT=0;REG_SOUNDCNT_H|=0x8000; }
    int press=g.pressed_latch;
    g.pressed_latch=0;
    if(g.in_menu) {
        if(press&(KEY_A|KEY_START)) {
            g.chocolates=g.deaths=0;g.have_slide=g.have_dash=g.have_blink=0;g.in_menu=0;load_room(0,0);
        }
        return;
    }
    if(g.ending) {
        ++g.ending_timer;
        if(g.true_ending&&g.ending_timer>600) {g.true_ending=0;g.ending_timer=0;}
        else if(!g.true_ending&&g.ending_timer>180) enter_menu();
        return;
    }
    if(g.paused) {
        if(press&KEY_START) { leave_pause(); return; }
        if(press&KEY_UP) g.pause_selection=(uint8_t)((g.pause_selection+2)%3);
        if(press&KEY_DOWN) g.pause_selection=(uint8_t)((g.pause_selection+1)%3);
        if(press&KEY_A) {
            if(g.pause_selection==0) leave_pause();
            else if(g.pause_selection==1) load_room(g.room,1);
            else enter_menu();
        }
        return;
    }
    if(press&KEY_START) { enter_pause(); return; }
    if(press&KEY_R) {
        g.have_slide=g.have_dash=g.have_blink=1;
        load_room((uint8_t)(g.room+1),0);return;
    }
    if(g.blink_sfx_cooldown) --g.blink_sfx_cooldown;
    if(g.blink_effect_timer)--g.blink_effect_timer;
    if(g.title_timer)--g.title_timer;
    if((g.room==1||g.room==2)&&!g.title_timer&&!g.paint_variants_loaded) {
        dma16(paint_tiles,OBJ_VRAM+1536,2560);
        if(g.room==2) {
            dma16(kitchen_tiles,OBJ_VRAM+4096,KITCHEN_TILE_HALFWORDS);
            dma16(kitchen_palette,OBJ_PALETTE+96,16);
        }
        g.paint_variants_loaded=1;
    }
    for(unsigned i=0;i<10;i++)if(trails[i].age)--trails[i].age;
    if(g.dead) {
        unsigned n=group_count[9]; g.frame=group_start[9]+(g.death_timer/3<n?g.death_timer/3:n-1);
        if(++g.death_timer>=54) load_room(g.room,1);
        return;
    }
    int left=(g.keys&KEY_LEFT)!=0, right=(g.keys&KEY_RIGHT)!=0, down=(g.keys&KEY_DOWN)!=0;
    int wx=g.x>>8, wy=g.y>>8;
    g.on_ground=player_hits(r,wx,wy+1) || platform_at(r,wx,wy+13);
    int32_t requested_vx=(right-left)*1024;
    if(down&&g.on_ground&&(g.sliding||!requested_vx||g.have_slide)) g.low_profile=1;
    else if(g.low_profile&&!player_hits_shape(r,wx,wy,0)) g.low_profile=0;
    if(g.on_ground) g.coyote=6;
    else if(g.coyote) --g.coyote;
    if(press&KEY_A) g.jump_buffer=4;
    /* Do not stand up or walk Elli into a ceiling after a slide. */
    if(g.low_profile&&!down&&player_hits_shape(r,wx,wy,0)) requested_vx=0;
    int locked=g.wall_lock!=0;
    if(locked) {g.vx=(int32_t)g.wall_push*256;--g.wall_lock;g.sliding=0;}
    else if(g.sliding) {
        if(!down || !g.on_ground || !g.have_slide) g.sliding=0;
        else if(g.vx<0) { g.vx+=13; if(g.vx>0)g.vx=0; }
        else if(g.vx>0) { g.vx-=13; if(g.vx<0)g.vx=0; }
    }
    if(!g.sliding&&!locked) {
        g.vx=requested_vx;
        if(down && g.on_ground && g.vx && g.have_slide) g.sliding=1;
    }
    if(g.vx) g.facing=g.vx<0;
    if(g.on_ground || player_hits(r,wx+2,wy) || player_hits(r,wx-2,wy)) g.dash_spent=0;
    if(!g.dash && (press&KEY_B) && g.vx && !g.on_ground && g.have_dash && !g.dash_spent) g.dash=11;
    if(g.dash) {
        add_player_trail(0);
        g.vx=(g.facing?-1:1)*2560; g.vy=0;
        if(--g.dash==0) g.dash_spent=1;
    }
    else {
        int jumped=0;
        g.vy+=154; if(g.vy>2560) g.vy=2560;
        int wr=g.room!=ROOM_COUNT-1&&player_hits(r,wx+1,wy),wl=g.room!=ROOM_COUNT-1&&player_hits(r,wx-1,wy);
        if(!g.on_ground&&g.room!=ROOM_COUNT-1) {
            if(wr){g.wall_coyote=6;g.wall_coyote_side=1;}
            else if(wl){g.wall_coyote=6;g.wall_coyote_side=-1;}
            else if(g.wall_coyote)--g.wall_coyote;
        } else {g.wall_coyote=0;g.wall_coyote_side=0;}
        if(!g.on_ground&&g.vy>0&&((wr&&right)||(wl&&left))) {
            g.vy=(g.vy*4)/7;
            g.facing=wl?1:0;
        }
        if(g.jump_buffer) {
            if(!g.on_ground&&g.wall_coyote&&((g.wall_coyote_side>0&&right)||(g.wall_coyote_side<0&&left))) {
                g.vy=-2560;g.vx=g.wall_coyote_side>0?-1024:1024;
                g.wall_push=g.wall_coyote_side>0?-4:4;g.wall_lock=10;g.wall_coyote=0;jumped=1;
            } else if(g.coyote) {
                g.vy=-2560; jumped=1;
            }
        }
        if(jumped) { g.jump_buffer=0; g.coyote=0; audio_play_sfx(0); }
        else if(g.jump_buffer) --g.jump_buffer;
        /* Do not cancel a newly buffered jump in the same tick. A very short
           tap still receives one complete simulation step of upward motion. */
        if(!jumped && !(g.keys&KEY_A) && g.vy<0) g.vy=0;
    }
    if(!g.dash&&g.sliding)add_player_trail(1);
    move_x(r,g.vx); move_y(r,g.vy);
    update_enemies();
    if(g.room==ROOM_COUNT-1&&enemy_count&&enemies[0].a.behavior==12&&enemies[0].a.state==-2) {
        const EnemySpriteAsset* death=&enemy_sprite_assets[enemies[0].a.sprite];
        if(enemies[0].frame+1>=death->frames&&++enemies[0].a.timer>45){enter_ending();return;}
    }
    wx=g.x>>8; wy=g.y>>8;
    g.on_ground=player_hits(r,wx,wy+1) || platform_at(r,wx,wy+13);
    int blinking=down && g.on_ground && !g.vx && g.have_blink && g.blink_left;
    g.blinking=(uint8_t)blinking;
    if(blinking) {
        --g.blink_left;
        if(player_hits_hazard(r,wx,wy) && !g.blink_sfx_cooldown) { audio_play_sfx(3); g.blink_sfx_cooldown=12;g.blink_effect_timer=24; }
    } else if(!down || !g.on_ground || g.vx) {
        g.blink_left=(g.room==ROOM_COUNT-1)?15:30;
    }
    int enemy_hit=player_hits_enemy(wx,wy);
    if(blinking&&enemy_hit&&!g.blink_sfx_cooldown){audio_play_sfx(3);g.blink_sfx_cooldown=12;g.blink_effect_timer=24;}
    if(((player_hits_hazard(r,wx,wy)||enemy_hit) && !blinking) || wy>r->h+32) {
        g.dead=1;g.low_profile=g.sliding=0; ++g.deaths; g.death_timer=0; g.vx=g.vy=0; g.group=9; g.frame=group_start[9]; audio_play_sfx(2);
        return;
    }
    uint8_t group=0;
    int wall_slide=g.room!=ROOM_COUNT-1&&!g.on_ground&&g.vy>0&&((player_hits(r,wx+1,wy)&&right)||(player_hits(r,wx-1,wy)&&left));
    int slope_near=g.vx&&g.vy>=0&&descending_slope_support(r,wx,wy,g.vx);
    if(slope_near) g.slope_anim_grace=4;
    else if(g.slope_anim_grace) --g.slope_anim_grace;
    int slope_run=!g.on_ground&&g.vy>=0&&g.vx&&g.slope_anim_grace;
    if(blinking) group=8;
    else if(g.dash) group=7;
    else if(g.sliding) group=6;
    else if(g.low_profile&&g.on_ground) group=5;
    else if(wall_slide) group=4;
    else if(!g.on_ground&&g.vy<0) group=2;
    else if(slope_run) group=1;
    else if(!g.on_ground&&g.vy>=0) group=3;
    else if(g.vx) group=1;
    if(group!=g.group) { g.group=group; g.anim_tick=0; }
    if(++g.anim_tick>=5) { g.anim_tick=0; g.frame=group_start[group]+((g.frame-group_start[group]+1)%group_count[group]); }
    else if(g.frame<group_start[group] || g.frame>=group_start[group]+group_count[group]) g.frame=group_start[group];
    g.sign=0;
    for(unsigned i=0;i<r->sign_count;i++) {
        const SignAsset* s=&sign_assets[r->sign_first+i];
        if(wx>=s->x-10&&wx<=s->x+26&&wy>=s->y-20&&wy<=s->y+20) { g.sign=s->kind; break; }
    }
    for(unsigned i=0;i<r->pickup_count && i<32;i++) {
        if(g.pickup_collected&(1u<<i)) continue;
        const PickupAsset* p=&pickup_assets[r->pickup_first+i];
        int player_top=g.low_profile?wy-3:wy-12;
        /* Collectibles are native 16x16 sprites centred on their instance.
           Use the actual player profile instead of the old oversized proximity
           box, which could play the coin sound well before visible contact. */
        if(wx+7>=p->x-8&&wx-7<=p->x+7&&wy+12>=p->y-8&&player_top<=p->y+7) {
            g.pickup_collected|=1u<<i;
            audio_play_sfx(1);
            if(p->kind==0) ++g.chocolates;
            else if(p->kind==1) g.have_slide=1;
            else if(p->kind==2) g.have_dash=1;
            else if(p->kind==3) g.have_blink=1;
        }
    }
    if(wx+7>=r->next_x && wx-7<r->next_x+r->next_w && wy+12>=r->next_y && wy-12<r->next_y+r->next_h && g.room+1<ROOM_COUNT) {
        load_room(g.room+1,0); return;
    }
    int cx=wx-120, cy=wy-80;
    if(cx<0)cx=0;
    if(cy<0)cy=0;
    if(cx>r->w-240)cx=r->w-240;
    if(cy>r->h-160)cy=r->h-160;
    if(cx<0)cx=0;
    if(cy<0)cy=0;
    g.camera_x=(int16_t)cx; g.camera_y=(int16_t)cy;
}

static void put_pixel(volatile uint16_t* page,int x,int y,uint8_t colour) {
    volatile uint16_t* dst=page+y*120+(x>>1); uint16_t old=*dst;
    *dst=(x&1)?(uint16_t)((old&0x00FF)|(colour<<8)):(uint16_t)((old&0xFF00)|colour);
}
static int32_t render_lerp(int32_t previous,int32_t current) {
    return (previous*(4-render_alpha)+current*render_alpha)/4;
}
static void draw_boss_patch(volatile uint16_t* page,const RuntimeEnemy* e) {
    unsigned kind=0;
    while(kind<4&&e->a.sprite!=boss_sprite_ids[kind])++kind;
    if(kind>=4)return;
    const EnemySpriteAsset* sprite=&enemy_sprite_assets[e->a.sprite];
    const BossPatchAsset* patch=&boss_patch_assets[boss_patch_first[kind]+(e->frame%sprite->frames)];
    int left=(int)patch->x-render_camera_x,top=(int)patch->y-render_camera_y;
    int x0=left<0?0:left,y0=top<0?0:top,x1=left+patch->w>240?240:left+patch->w,y1=top+patch->h>160?160:top+patch->h;
    if(x0>=x1||y0>=y1)return;
    int sx=x0-left,bulk=(x1-x0)&~1;
    const unsigned char* pixels=(sx&1)?boss_patch_pixels_odd:boss_patch_pixels;
    int source_x=(sx&1)?sx-1:sx;
    for(int y=y0;y<y1;y++) {
        int sy=y-top;const unsigned char* src=pixels+patch->offset+(uint32_t)sy*patch->w+source_x;
        if(bulk)dma16(src,page+y*120+(x0>>1),(uint32_t)bulk>>1);
        if((x1-x0)&1)put_pixel(page,x0+bulk,y,boss_patch_pixels[patch->offset+(uint32_t)sy*patch->w+sx+bulk]);
    }
}
static IWRAM_CODE void draw_enemy(volatile uint16_t* page,const RuntimeEnemy* e) {
    const EnemySpriteAsset* s=&enemy_sprite_assets[e->a.sprite];
    int ax=iabs(e->a.scale_x),ay=iabs(e->a.scale_y); if(!ax||!ay)return;
    int ex=render_lerp(e->previous_x,e->a.x)>>8,ey=render_lerp(e->previous_y,e->a.y)>>8;
    int left=ex-((e->a.scale_x<0?s->w-s->ox:s->ox)*ax>>8)-render_camera_x;
    int top=ey-((e->a.scale_y<0?s->h-s->oy:s->oy)*ay>>8)-render_camera_y;
    int dw=(s->w*ax)>>8,dh=(s->h*ay)>>8;
    if(left+dw<=0||left>=240||top+dh<=0||top>=160)return;
    int x0=left<0?0:left,y0=top<0?0:top,x1=left+dw>240?240:left+dw,y1=top+dh>160?160:top+dh;
    unsigned frame_index=e->frame%s->frames;
    const unsigned char* frame=enemy_pixels+s->offset+(uint32_t)frame_index*s->w*s->h;
    if(ax==256&&ay==256) {
        for(int y=y0;y<y1;y++) {
            int sy=e->a.scale_y<0?s->h-1-(y-top):y-top;
            const unsigned char* row=frame+(uint32_t)sy*s->w;
            const unsigned char* span=enemy_spans+s->spans+((uint32_t)frame_index*s->h+sy)*4;
            int sl=span[0]|(span[1]<<8),sr=span[2]|(span[3]<<8);if(sl==0xffff)continue;
            int row_x0=e->a.scale_x<0?left+s->w-sr:left+sl;
            int row_x1=e->a.scale_x<0?left+s->w-sl:left+sr;
            if(row_x0<x0)row_x0=x0;
            if(row_x1>x1)row_x1=x1;
            int x=row_x0;
            if((x&1)&&x<x1) {int sx=e->a.scale_x<0?s->w-1-(x-left):x-left;uint8_t c=row[sx];if(c)put_pixel(page,x,y,c);++x;}
            for(;x+1<x1;x+=2) {
                int sx0=e->a.scale_x<0?s->w-1-(x-left):x-left;
                int sx1=e->a.scale_x<0?s->w-1-(x+1-left):x+1-left;
                uint8_t c0=row[sx0],c1=row[sx1];if(!c0&&!c1)continue;
                volatile uint16_t* dst=page+y*120+(x>>1);
                if(c0&&c1) *dst=(uint16_t)c0|((uint16_t)c1<<8);
                else {
                    uint16_t old=*dst;
                    *dst=(uint16_t)(c0?c0:(old&255))|(uint16_t)((c1?c1:(old>>8))<<8);
                }
            }
            if(x<x1){int sx=e->a.scale_x<0?s->w-1-(x-left):x-left;uint8_t c=row[sx];if(c)put_pixel(page,x,y,c);}
        }
        return;
    }
    for(int y=y0;y<y1;y++) {
        int sy=(y-top)*256/ay; if(e->a.scale_y<0)sy=s->h-1-sy;
        for(int x=x0;x<x1;x++) {
            int sx=(x-left)*256/ax; if(e->a.scale_x<0)sx=s->w-1-sx;
            uint8_t colour=frame[(uint32_t)sy*s->w+sx]; if(colour)put_pixel(page,x,y,colour);
        }
    }
}
static ROM_CODE void draw_rotating_trash(volatile uint16_t* page,const RuntimeEnemy* e) {
    /* Rotation frames and opaque spans are generated once on the host. */
    unsigned angle=(unsigned)(e->a.direction/5)%72u;
    int ex=(render_lerp(e->previous_x,e->a.x)>>8)-render_camera_x;
    int ey=(render_lerp(e->previous_y,e->a.y)>>8)-render_camera_y;
    int left=ex-TRASH_ROT_OX,top=ey-TRASH_ROT_OY;
    int y0=top<0?0:top,y1=top+TRASH_ROT_H>160?160:top+TRASH_ROT_H;
    const unsigned char* frame=trash_rot_pixels+(uint32_t)angle*TRASH_ROT_W*TRASH_ROT_H;
    const unsigned char* spans=trash_rot_spans+(uint32_t)angle*TRASH_ROT_H*4;
    for(int y=y0;y<y1;y++) {
        int sy=y-top;const unsigned char* span=spans+sy*4;
        int sl=span[0]|(span[1]<<8),sr=span[2]|(span[3]<<8);if(sl==0xffff)continue;
        int x0=left+sl<0?0:left+sl,x1=left+sr>240?240:left+sr;if(x0>=x1)continue;
        const unsigned char* row=frame+(uint32_t)sy*TRASH_ROT_W;int x=x0;
        if((x&1)&&x<x1){uint8_t c=row[x-left];if(c)put_pixel(page,x,y,c);++x;}
        for(;x+1<x1;x+=2) {
            uint8_t c0=row[x-left],c1=row[x+1-left];if(!c0&&!c1)continue;
            volatile uint16_t* dst=page+y*120+(x>>1);
            if(c0&&c1)*dst=(uint16_t)c0|((uint16_t)c1<<8);
            else {uint16_t old=*dst;*dst=(uint16_t)(c0?c0:(old&255))|(uint16_t)((c1?c1:(old>>8))<<8);}
        }
        if(x<x1){uint8_t c=row[x-left];if(c)put_pixel(page,x,y,c);}
    }
}
static inline const KitchenSpriteAsset* kitchen_sprite(uint16_t sprite_id) {
    unsigned index=(unsigned)sprite_id-KITCHEN_SPRITE_FIRST;
    return index<KITCHEN_SPRITE_COUNT?&kitchen_sprite_assets[index]:0;
}
static inline const KitchenSpriteAsset* kitchen_hardware_sprite(const RuntimeEnemy* e) {
    /* Hardware OBJs are deliberately a 1:1 path. Scaled projectiles stay on
       the software renderer, which preserves their original 0.5 scale. */
    if((e->a.scale_x!=256&&e->a.scale_x!=-256) ||
       (e->a.scale_y!=256&&e->a.scale_y!=-256))return 0;
    return kitchen_sprite(e->a.sprite);
}
static void draw_screen_sprite(volatile uint16_t* page,uint16_t sprite_id,uint8_t frame,int x,int y) {
    const EnemySpriteAsset* s=&enemy_sprite_assets[sprite_id];RuntimeEnemy e;
    e.a.sprite=sprite_id;e.a.scale_x=e.a.scale_y=256;e.frame=frame;e.active=1;
    e.a.x=e.previous_x=(int32_t)(render_camera_x+x+s->ox)<<8;
    e.a.y=e.previous_y=(int32_t)(render_camera_y+y+s->oy)<<8;
    draw_enemy(page,&e);
}
static IWRAM_CODE void draw_hud_number(volatile uint16_t* page,unsigned value,int x,int y) {
    unsigned tens=value/10;
    if(tens) {
        const unsigned char* glyph=hud_digits+(tens%10)*144;
        for(int py=0;py<12;py++) for(int px=0;px<12;px++) if(glyph[py*12+px]) put_pixel(page,x+px,y+py,255);
        x+=10;
    }
    const unsigned char* glyph=hud_digits+(value%10)*144;
    for(int py=0;py<12;py++) for(int px=0;px<12;px++) if(glyph[py*12+px]) put_pixel(page,x+px,y+py,255);
}

static void render(void) {
    volatile uint16_t* back=(volatile uint16_t*)(displayed_page?0x06000000:0x0600A000);
    if(g.in_menu) {
        const unsigned char* frame=menu_pixels;
        dma16(frame,back,19200);vblank();
        for(unsigned i=0;i<128;i++)OAM[i*4]=160;
        REG_BLDCNT=0;
        displayed_page^=1;REG_DISPCNT=(uint16_t)(0x1444|(displayed_page?0x10:0));return;
    }
    if(g.paused) {
        const unsigned char* frame=pause_pixels+(uint32_t)g.pause_selection*38400;
        dma16(frame,back,19200);vblank();
        for(unsigned i=0;i<128;i++)OAM[i*4]=160;
        REG_BLDCNT=0;
        displayed_page^=1;REG_DISPCNT=(uint16_t)(0x1444|(displayed_page?0x10:0));return;
    }
    if(g.ending) {
        unsigned width=g.true_ending?474:384;
        unsigned offset=g.true_ending?384u*218u:0;
        unsigned source_x=g.true_ending?(unsigned)((g.ending_timer*17u/100u)>90?90:g.ending_timer*17u/100u):0;
        const unsigned char* frame=end_pixels+offset+source_x;
        for(unsigned y=0;y<160;y++)dma16(frame+y*width,back+y*120,120);
        vblank();for(unsigned i=0;i<128;i++)OAM[i*4]=160;REG_BLDCNT=0;
        displayed_page^=1;REG_DISPCNT=(uint16_t)(0x1444|(displayed_page?0x10:0));return;
    }
    const RoomAsset* r=&room_assets[g.room];
    render_camera_x=(int16_t)render_lerp(g.previous_camera_x,g.camera_x);
    render_camera_y=(int16_t)render_lerp(g.previous_camera_y,g.camera_y);
    int cached_boss=g.room==ROOM_COUNT-1&&enemy_count&&enemies[0].active;
    if(!cached_boss) {
        const unsigned char* pixels=(render_camera_x&1)?room_pixels_odd:room_pixels;
        int source_x=(render_camera_x&1)?render_camera_x-1:render_camera_x;
        const unsigned char* src=pixels+r->pixels+(uint32_t)render_camera_y*r->w+source_x;
        for(int y=0;y<160;y++) dma16(src+(uint32_t)y*r->w,back+y*120,120);
    } else draw_boss_patch(back,&enemies[0]);
    for(unsigned i=0;i<enemy_count;i++)if(enemies[i].active&&enemies[i].a.behavior!=12&&
        !(g.paint_variants_loaded&&((enemies[i].a.flags&6)||(g.room==2&&kitchen_hardware_sprite(&enemies[i]))))) {
        if(enemies[i].a.behavior==8)draw_rotating_trash(back,&enemies[i]);
        else draw_enemy(back,&enemies[i]);
    }
    if(g.blink_effect_timer)draw_screen_sprite(back,blink_sprite_ids[g.room],(uint8_t)((24-g.blink_effect_timer)/3),
        (render_lerp(g.previous_x,g.x)>>8)-render_camera_x-32,(render_lerp(g.previous_y,g.y)>>8)-render_camera_y-32);
    if(!g.dead) { draw_hud_number(back,g.chocolates,25,11); draw_hud_number(back,g.deaths,196,142); }
    int reading=g.sign && (g.keys&KEY_DOWN) && g.vx==0;
    if(reading) {
        const unsigned char* dialog=dialog_data+(uint32_t)(g.sign-1)*240*77;
        for(int y=0;y<77;y++) dma16(dialog+y*240,back+(42+y)*120,120);
    }
    vblank();
    dma16(player_tiles+(uint32_t)g.frame*512,OBJ_VRAM,256);
    int relative_x=render_lerp(g.previous_x-((int32_t)g.previous_camera_x<<8),g.x-((int32_t)g.camera_x<<8))>>8;
    int relative_y=render_lerp(g.previous_y-((int32_t)g.previous_camera_y<<8),g.y-((int32_t)g.camera_y<<8))>>8;
    int sx=relative_x-15, sy=relative_y-16;
    OAM[0]=(uint16_t)(sy&255); OAM[1]=(uint16_t)(sx&511)|0x8000|(g.facing?0x1000:0); OAM[2]=512;
    if(g.sign && !reading) {
        int bx=relative_x-8, by=relative_y-33;
        OAM[4]=(uint16_t)(by&255); OAM[5]=(uint16_t)(bx&511)|0x4000; OAM[6]=576|(1<<12);
    } else OAM[4]=160;
    if(g.sign) {
        const RoomAsset* rr=&room_assets[g.room]; const SignAsset* a=0;
        for(unsigned i=0;i<rr->sign_count;i++) if(sign_assets[rr->sign_first+i].kind==g.sign) { a=&sign_assets[rr->sign_first+i]; break; }
        if(a) { OAM[8]=(uint16_t)((a->y-render_camera_y)&255); OAM[9]=(uint16_t)((a->x-render_camera_x)&511)|0x4000; OAM[10]=580|(2<<12); }
        else OAM[8]=160;
    } else OAM[8]=160;
    unsigned oam_index=3;
    for(unsigned i=0;i<r->pickup_count && i<32;i++) {
        if(g.pickup_collected&(1u<<i)) continue;
        const PickupAsset* p=&pickup_assets[r->pickup_first+i];
        int px=(int)p->x-render_camera_x-8, py=(int)p->y-render_camera_y-8;
        if(px<=-16||px>=240||py<=-16||py>=160) continue;
        unsigned a=oam_index*4;
        OAM[a]=(uint16_t)(py&255); OAM[a+1]=(uint16_t)(px&511)|0x4000;
        OAM[a+2]=(uint16_t)(584+p->kind*4)|(3<<12);
        if(++oam_index>=128) break;
    }
    for(;oam_index<16;oam_index++) OAM[oam_index*4]=160;
    for(unsigned i=16;i<64;i++)OAM[i*4]=160;
    if((g.room==1||g.room==2)&&g.paint_variants_loaded) {
        unsigned oi=16;
        for(unsigned i=0;i<enemy_count&&oi<64;i++) {
            const RuntimeEnemy* e=&enemies[i];if(!e->active)continue;
            const KitchenSpriteAsset* kitchen=g.room==2&&!(e->a.flags&6)?kitchen_hardware_sprite(e):0;
            if(!(e->a.flags&6)&&!kitchen)continue;
            int little=(e->a.flags&4)!=0;
            int ex=render_lerp(e->previous_x,e->a.x)>>8,ey=render_lerp(e->previous_y,e->a.y)>>8;
            int width=kitchen?kitchen->w:(little?16:32),height=kitchen?kitchen->h:(little?16:32);
            int origin_x=kitchen?(e->a.scale_x<0?width-kitchen->ox:kitchen->ox):(little?8:16);
            int x=ex-render_camera_x-origin_x;
            int y=ey-render_camera_y-(kitchen?kitchen->oy:(little?8:16));
            if(x<=-width||x>=240||y<=-height||y>=160)continue;
            unsigned entry=oi++,a=entry*4;
            if(!kitchen&&!little&&e->tilt&&entry<48) {
                unsigned matrix=entry-16;int angle=e->tilt;
                int cosine=256-(angle*angle)/24,sine=angle*4;
                OAM[a]=(uint16_t)(y&255)|0x0100;
                OAM[a+1]=(uint16_t)(x&511)|0x8000|(matrix<<9);
                OAM[matrix*16+3]=(uint16_t)cosine;OAM[matrix*16+7]=(uint16_t)-sine;
                OAM[matrix*16+11]=(uint16_t)sine;OAM[matrix*16+15]=(uint16_t)cosine;
            } else {
                unsigned shape=height==64?0x8000:0;
                /* shape=vertical,size=3 is 32x64. Square 16x16 and 32x32
                   retain size=1 and size=2 respectively. */
                unsigned size=height==64?0xC000:(width==16?0x4000:0x8000);
                OAM[a]=(uint16_t)(y&255)|shape;OAM[a+1]=(uint16_t)(x&511)|size|(e->a.scale_x<0?0x1000:0);
            }
            unsigned variant=i&3;
            unsigned tile=kitchen?(unsigned)kitchen->tile+(unsigned)(e->frame&1)*(unsigned)kitchen->frame_tiles:
                (little?736u+variant*8u+e->frame*4u:608u+variant*32u+e->frame*16u);
            OAM[a+2]=(uint16_t)(tile|((kitchen?6:5)<<12));
        }
    }
    unsigned trail_oi=64;int trails_visible=0;
    for(unsigned i=0;i<10&&trail_oi<74;i++) {
        if(!trails[i].age)continue;
        int x=(trails[i].x>>8)-render_camera_x-15,y=(trails[i].y>>8)-render_camera_y-16;
        if(x<=-32||x>=240||y<=-32||y>=160)continue;
        unsigned a=trail_oi++*4;
        OAM[a]=(uint16_t)(y&255)|0x0400;
        OAM[a+1]=(uint16_t)(x&511)|0x8000|(trails[i].facing?0x1000:0);
        OAM[a+2]=(uint16_t)(g.room==1?768:528);
        trails_visible=1;
    }
    for(;trail_oi<74;trail_oi++)OAM[trail_oi*4]=160;
    if(trails_visible && loaded_trail_frame!=trail_effect_frame) {
        volatile uint16_t* trail_vram=g.room==1?OBJ_VRAM+4096:OBJ_VRAM+256;
        dma16(trail_tiles+(uint32_t)trail_effect_frame*512,trail_vram,256);loaded_trail_frame=trail_effect_frame;
    }
    if(!g.dead) {
        OAM[126*4]=7; OAM[126*4+1]=7|0x4000; OAM[126*4+2]=584|(3<<12);
        OAM[127*4]=139; OAM[127*4+1]=178|0x4000; OAM[127*4+2]=600|(4<<12);
    } else { OAM[126*4]=160; OAM[127*4]=160; }
    for(unsigned i=90;i<110;i++)OAM[i*4]=160;
    if(g.room==ROOM_COUNT-1&&enemy_count&&!g.dead) {
        int life=enemies[0].a.vertical_speed;if(life<0)life=0;if(life>10)life=10;
        for(unsigned layer=0;layer<2;layer++)for(unsigned i=0;i<5;i++) {
            unsigned entry=(layer?105:90)+i,a=entry*4,x=(unsigned)(-15+(int)i*64)&511;
            int narrow=i==4;OAM[a]=136|(narrow?0x8000:0x4000);
            /* Horizontal size=3 is 64x32; vertical size=2 is 16x32. */
            OAM[a+1]=(uint16_t)x|(narrow?0x8000:0xC000);
            OAM[a+2]=(uint16_t)(bossbar_tile[(layer?5:0)+i]|(7<<12));
        }
        for(int i=0;i<life;i++) {
            /* Horizontal size=2 is the intended 32x16 life segment. */
            unsigned a=(95u+(unsigned)i)*4;OAM[a]=143|0x4000;OAM[a+1]=(uint16_t)(3+i*23)|0x8000;
            OAM[a+2]=(uint16_t)(bossbar_tile[10]|(7<<12));
        }
    }
    if(g.title_timer&&g.room<ROOM_COUNT-1) {
        const TitleAsset* title=&title_assets[g.room];
        int anchor_x=g.room==2?96-render_camera_x:120;
        int anchor_y=g.room==2?400-render_camera_y:80;
        for(unsigned i=0;i<3;i++) {
            unsigned a=(116+i)*4;
            if(i<title->blocks) {
                int x=anchor_x+title->x_offset+(int)i*64,y=anchor_y+title->y_offset;
                OAM[a]=(uint16_t)(y&255)|0x2400;OAM[a+1]=(uint16_t)(x&511)|0xC000;OAM[a+2]=(uint16_t)(608+i*128);
            } else OAM[a]=160;
        }
        for(unsigned i=0;i<4;i++) {
            unsigned a=(119+i)*4;
            if(i<title->bottom_count) {
                int x=anchor_x+title->x_offset+title->bottom_x+(int)i*32,y=anchor_y+title->y_offset+64;
                OAM[a]=(uint16_t)(y&255)|0x6400;OAM[a+1]=(uint16_t)(x&511)|0x4000;OAM[a+2]=(uint16_t)(992+i*8);
            } else OAM[a]=160;
        }
        unsigned eva=(g.title_timer*16u+49u)/50u;if(eva>16)eva=16;
        /* Only title entries use semi-transparent OBJ mode (attr0 bit 10).
           Semi-transparent OBJs are implicit alpha source A; selecting OBJ
           globally here would also fade Elli, pickups and both HUD sprites. */
        REG_BLDCNT=0x0400;REG_BLDALPHA=(uint16_t)(((16-eva)<<8)|eva);
    } else {
        for(unsigned i=116;i<=122;i++)OAM[i*4]=160;
        if(trails_visible){REG_BLDCNT=0x0400;REG_BLDALPHA=(10u<<8)|6u;}
        else REG_BLDCNT=0;
    }
    displayed_page^=1; REG_DISPCNT=(uint16_t)(0x1444|(displayed_page?0x10:0));
}

__attribute__((section(".iwram"))) int main(void) {
    /* Fast sequential Game Pak timing plus prefetch. The complete hot loop is
       copied to IWRAM at boot, so control, collision and DMA setup no longer
       pay ROM wait states on every instruction. */
    REG_WAITCNT=0x4317;
    REG_DISPCNT=0; REG_BG2PA=0x100; REG_BG2PB=0; REG_BG2PC=0; REG_BG2PD=0x100; REG_BG2X=REG_BG2Y=0;
    for(int i=0;i<128;i++) OAM[i*4]=160;
    dma16(obj_palette,OBJ_PALETTE,96); dma16(button_tiles,OBJ_VRAM+1024,64); dma16(sign_tiles,OBJ_VRAM+1088,64);
    dma16(paint_base_tiles,OBJ_VRAM+256,640);
    dma16(pickup_tiles,OBJ_VRAM+1152,256);
    dma16(skull_tiles,OBJ_VRAM+1408,64);
    dma16(title_palette,OBJ_PALETTE+96,160);
    audio_init(); g.facing=0; load_room(0,0); enter_menu(); REG_DISPCNT=0x1444;
    uint32_t display_frame=0;
    for(;;) {
        uint16_t sampled=(uint16_t)(~REG_KEYINPUT)&0x03FF;
        g.pressed_latch|=sampled&~g.keys;
        g.keys=sampled;
        unsigned phase=display_frame++&3;
        render_alpha=(uint8_t)(3-phase);
        if(phase) {
            /* Preserve the exact 45 Hz simulation, but interpolate its 24.8
               positions over all four 60 Hz display frames. */
            g.previous_x=g.x;g.previous_y=g.y;
            g.previous_camera_x=g.camera_x;g.previous_camera_y=g.camera_y;
            for(unsigned i=0;i<enemy_count;i++) {
                enemies[i].previous_x=enemies[i].a.x;
                enemies[i].previous_y=enemies[i].a.y;
            }
            update();
        }
        render();
    }
}
