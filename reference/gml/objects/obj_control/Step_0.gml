/// @description pause
pause_k = keyboard_check_pressed(vk_escape)|| (gamepad_button_check_pressed(0,gp_start))

if pause_k and room != rm_language and room != rm_menu 
and room != rm_boss_scene and room != rm_end and room != rm_credits and 
!instance_exists(obj_fade) and instance_exists(obj_player){
    if obj_player.dead = 0{
        if global.pause = 0{
            global.pause = 1
        } else global.pause = 0
    }
}

if global.pause = 1{
    if !instance_exists(obj_pause){
        instance_create(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+72,obj_pause)
    }
}


/// General
if global.plus = 1{
    global.have_slide = true
    global.have_dash = true
    global.have_blink = true
}
if global.end_game = 1{
    scr_savegame()
}
if room = rm_menu{
    scr_loadgame()
}




/// BestTime
if room = rm_end{

if global.time < global.besttime{
global.besttime = global.time
global.bestsec = global.sec
global.bestminute = global.minute
global.bestmilisec = global.milisec
}
if global.besttime = 0{
global.besttime = global.time
global.bestsec = global.sec
global.bestminute = global.minute
global.bestmilisec = global.milisec
}
}
global.time = global.sec+global.minute+global.milisec


