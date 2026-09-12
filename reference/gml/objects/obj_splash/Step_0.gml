if image_alpha = 0 and dir = 1{
    instance_destroy()
    room_goto_next()
}else if image_alpha > 1{
timer --
if timer < 0{
dir = 1
}
}
if dir = 0{
image_alpha +=0.025
}else image_alpha -=0.025

