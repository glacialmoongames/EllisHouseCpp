/// @description Variaveis
// reset
global.touch_left = 0;
global.touch_right = 0;
global.touch_up = 0;
global.touch_down = 0;

global.touch_jump = 0;
global.touch_jump_pressed = 0;

down_k = 0
//particles
sname = 0
part = 3
//animation speed
image_speed = 0.2;

//moviment
grav = 0.6;
hsp = 0;
vsp = 0;
truejumpspd = 10;
jumpspeed = truejumpspd;
truemovespd = 4;
movespeed = truemovespd;
move_lock = false;
grounded = false;

//controller dead zone
gamepad_set_axis_deadzone(0,0.3)
//slide
sfx_s =0
//dash
dash = 0
dash_c = 0

//crouch
crouch = 0

//blink
true_blink = 30
blink = true_blink
btime = 0

//death
dead = 0

// respawn
respawnx = 0
respawny = 0

// coyote time
buffer_counter = 0
buffer_max = 4
coyote_counter = 0
coyote_max = 6
jumped = true
on_ground = false

// tamanho tela
sizex = display_get_width()
sizey = display_get_height()

// create hud
if !instance_exists(obj_hud){
instance_create(x,y,obj_hud)
}




/// Skins
// POPE
if global.plus = 1 and global.skin = 1{
    sdeath = spr_pope_death
    sdash = spr_pope_dash
    sidle = spr_pope_idle
    srun = spr_pope_run
    sjump = spr_pope_jump
    swall = spr_pope_walljump    
    sfall = spr_pope_fall
    scrouch = spr_pope_crouch
    sslide = spr_pope_slide
    sblink = spr_pope_blink
}else if global.plus = 1 and global.skin = 0{
// MIB
    sdeath = spr_mib_death
    sdash = spr_mib_dash
    sidle = spr_mib_idle
    srun = spr_mib_run
    sjump = spr_mib_jump
    swall = spr_mib_walljump    
    sfall = spr_mib_fall
    scrouch = spr_mib_crouch
    sslide = spr_mib_slide
    sblink = spr_mib_blink
}else{
//NORMAL
    sdeath = spr_player_death
    sdash = spr_player_dash
    sidle = spr_player_idle
    srun = spr_player_run
    sjump = spr_player_jump
    swall = spr_player_walljump    
    sfall = spr_player_fall
    scrouch = spr_player_crouch
    sslide = spr_player_slide
    sblink = spr_player_blink
}


