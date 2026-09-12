//45.75 quantidade de vida pra ser tirada
// vida = 549
if global.pause = 0{
if room = rm_boss{
    timer ++
}

if instance_exists(obj_player){
if obj_player.dead =0{
image_speed = 0.2
if timer > timing and life != 183{
    if state !=3{
    sprite_index = spr_boss_hit
    instance_create(x,y,obj_boss_atk)
    }
    if state !=0{
    life -= 26.4910248987 // lose life
    }
    if state = 0 {
        timing = 60
        state = 1
    }else if state = 1{
        randomize()
        sfx = choose(sfx_boss2,sfx_boss2,sfx_bossroar)
        audio_play_sound(sfx,1,0)
        timing = 120
        state = 2
    }else if state = 2{
        randomize()
        sfx = choose(sfx_boss2,sfx_boss2,sfx_bossroar)
        audio_play_sound(sfx,1,0)
        timing = 120
        state = 3
    }else if state = 3{
        randomize()
        sfx = choose(sfx_boss2,sfx_boss2,sfx_bossroar)
        audio_play_sound(sfx,1,0)
        timing = 30
        attacked = 0    
        state = 0
    }
    timer = 0
    if state != 3{
        sprite_index = spr_boss_idle
    }
}

//state eye
if state = 2 {
    if sprite_index = spr_boss_idle and attacked = 0{
        image_index = 0
}
    if attacked = 0{
    sprite_index = spr_boss_eyeatk
    attacked = 1
    }
}
if sprite_index = spr_boss_eyeatk and image_index > 15{
    instance_create(295,50,obj_eyel)
    instance_create(250,50,obj_eyer)
    sprite_index = spr_boss_eyegrown
}else if sprite_index = spr_boss_eyegrown and image_index > 8{
    sprite_index = spr_boss_idle
}
}else {state = 0 sprite_index = spr_boss_idle attacked = 0 timer = 0 timing = 60}
}
}else {image_speed = 0}



// Death 
if life < 60 {
state = 0
timer = 0
sprite_index = spr_boss_death
if image_index > 25 {
image_speed = 0
if instance_exists(obj_player){
instance_create(obj_player.x,obj_player.y,obj_next)
}
}

}

