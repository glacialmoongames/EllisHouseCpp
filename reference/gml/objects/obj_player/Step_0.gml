/// @description Get input
if dead = 0 {
right_k = keyboard_check(ord("D"))
|| (gamepad_axis_value(0,gp_axislh) > 0)
|| gamepad_button_check(0,gp_padr)
|| global.touch_right;

left_k = -(
keyboard_check(ord("A"))
|| (gamepad_axis_value(0,gp_axislh) < 0)
|| gamepad_button_check(0,gp_padl)
|| global.touch_left
);

down_k = keyboard_check(ord("S"))
|| (gamepad_axis_value(0,gp_axislv) > 0)
|| gamepad_button_check(0,gp_shoulderl)
|| gamepad_button_check(0,gp_padd)
|| global.touch_down;

up_k = keyboard_check(ord("W"))
|| gamepad_button_check(0,gp_face3)
|| gamepad_button_check(0,gp_face2)
|| gamepad_button_check(0,gp_shoulderr)
|| global.touch_up;

jump_k = keyboard_check_pressed(vk_space)
|| gamepad_button_check_pressed(0,gp_face1)
|| global.touch_jump_pressed;

jumpheld_k = keyboard_check(vk_space)
|| gamepad_button_check(0,gp_face1)
|| global.touch_jump;
}
//if keyboard_check_pressed(vk_up){
//game_restart() //for tests
//}



/// State control
if global.pause = 0{
    if dead = 0{



// dash
if up_k and hsp != 0 and dash_c = false and grounded = false and global.have_dash = true{
dash+= 1
state = scr_dash()
}else if dead =0 and crouch = 0{ state = scr_move() }

if dash > 10 and dash_c = false and dash_c = false{
state = scr_move()
dash = 0
dash_c = true
}
if place_meeting(x,y+2,obj_colision) or place_meeting(x+2,y,obj_colision) or place_meeting(x-2,y,obj_colision){
dash = 0
dash_c = false
}

// slide crouch
if down_k and hsp = 0 and grounded = true and blink = 0 {
    state = scr_crouch()
    crouch = 1
}else if down_k and hsp !=0 and grounded = true and global.have_slide = true{
    state = scr_slide()
    /*if sfx_s = 0{
        audio_play_sound(sfx_slide,1,0)
        sfx_s = 1
    }*/
    crouch = 2
}else if down_k and hsp = 0 and grounded = true and blink != 0 {
    state = scr_blink()
    crouch = 1
}else { state = scr_move crouch = 0 blink = true_blink}
/*if sprite_index != sslide {
    sfx_s = 0
    audio_stop_sound(sfx_slide)
}*/
    }
}
if room = rm_boss{
    true_blink = 15
}else true_blink = 30

/* */
/// Animations
if global.pause = 0 and dead = 0{
// turn 

if hsp < 0 {
    image_xscale = -1
}
if hsp > 0 {
    image_xscale = 1
}

//Animations

if up_k and vsp = 0 and grounded = false and global.have_dash = true{
    sprite_index = sdash
    image_speed = 0.2
}else if hsp =0 and vsp =0 and crouch = 0{
    sprite_index = sidle
    image_speed = 0.2
} else if hsp !=0 and vsp =0 and crouch != 2 {
    sprite_index = srun
    image_speed = 0.3
} else if place_meeting(x + 1, y, obj_colision)and grounded = false  and vsp>0 and right_k and room != rm_boss {
    sprite_index = swall
    image_xscale = 1
    image_speed = 0.2
} else if place_meeting(x - 1, y, obj_colision)and grounded = false  and vsp>0 and -left_k and room != rm_boss {
    sprite_index = swall
    image_xscale = -1
    image_speed = 0.2
} else if vsp < 0{
    sprite_index = sjump
    image_speed = 0.2
} else if vsp > 0 and grounded = false{
    sprite_index = sfall
    image_speed = 0.2
} else if crouch = 1 and blink = 0 or global.have_blink = false and crouch = 1{
    sprite_index = scrouch
    image_speed = 0.2
} else if crouch = 2 and global.have_slide = true{
    sprite_index = sslide
    image_speed = 0.2
} else if crouch = 1 and global.have_blink = true{
    sprite_index = sblink
    image_speed = 0.2
}
}else if dead = 1 {
    sprite_index = sdeath
    image_speed = 0.2
} else image_speed = 0


/* */
/// death

if dead = 1 {
scr_dead()

}

//Normal
if sprite_index = sdeath and image_index > 16{
image_speed = 0
if !instance_exists(obj_fade){
instance_create(x,y,obj_fade)
global.deathcount += 1
}
}


/* */
/// passar
if place_meeting(x,y,obj_next)and dead = 0{
    instance_create(x,y,obj_fade)
    instance_destroy()
}

if room = rm_menu{
    instance_destroy()
}

/* */
///Particles
if grounded = true and part !=0{
part--
sname = part_system_create()
particle1 = part_type_create();
part_type_sprite(particle1,spr_part,0,0,0);
part_type_size(particle1,1,1,0,0);
part_type_scale(particle1,1,1);
part_type_alpha1(particle1,1);
part_type_speed(particle1,0.60,1,0,0);
part_type_direction(particle1,0,180,0,0);
part_type_orientation(particle1,0,0,0,0,1);
part_type_blend(particle1,0);
part_type_life(particle1,10,30);
part_particles_create(sname,x,y+10,particle1,2)
}else if grounded = false {part = 3}



/* */
/*  */
