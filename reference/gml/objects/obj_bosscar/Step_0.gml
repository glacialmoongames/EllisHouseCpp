
scr_things()
if global.pause = 0{
if image_xscale < 1.1{
    image_xscale += 0.05
    image_yscale += 0.05
}
image_angle = direction
timer ++
if timer > 30 and state = 0 {
    timer = 0
    state = 1
}
if state = 0{
y -=2.5
x +=2.5
}else
if state = 1{
    if instance_exists(obj_player){
        if timer < 20 {
            direction = point_direction(x,y,obj_player.x,obj_player.y)
            }else speed = spd
        }
    }
}else speed = 0

