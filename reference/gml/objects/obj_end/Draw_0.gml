
if true_final = 1{
draw_sprite_ext(spr_trueend,0,anim,0,1,1,0,c_white,1)
}else draw_sprite_ext(spr_end,0,0,0,1,1,0,c_white,1)
timer ++
if timer > 180 and true_final = 0{
room = rm_menu
} else if timer > 600 and true_final = 1{
instance_create(0,0,obj_fadeinlong)
true_final = 0
timer = 0
}
if timer > 550{
instance_create(0,0,obj_fadeout)
}
if anim > -90{
anim -=0.17
}
global.end_game = 1


