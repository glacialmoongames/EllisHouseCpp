scr_things()
if global.pause = 0{

if timer > 15 and timer <17{
direction = point_direction(x,y,obj_player.x,obj_player.y)
image_angle = direction-270
speed = spd
}
timer ++
}else speed = 0

