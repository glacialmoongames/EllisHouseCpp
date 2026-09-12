
state = 0
timer = 0
type = 1
spd = 8
image_speed = 0.3
if instance_exists(obj_player)and sprite_index = spr_fireball 
{image_angle = point_direction(x,y,obj_player.x,obj_player.y)}
image_xscale = 0.5
image_yscale = 0.5

