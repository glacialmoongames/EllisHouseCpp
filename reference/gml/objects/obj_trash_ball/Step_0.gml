if global.pause = 0{
image_angle -=5
scr_things()
x += spd
if !place_meeting(x,y,obj_colision){
    y+=2
}
}

