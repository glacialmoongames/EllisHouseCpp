if global.pause = 0{
speed = 2
move_bounce_solid(1)
}else speed = 0
scr_espawner()
if squishy = 1{
    stime --
    image_yscale -=0.1
    image_xscale -=0.1
    if stime = 0{
        squishy = 0
        stime = 5
    }
}
if squishy = 0 and image_yscale !=1{
    image_yscale +=0.1
    image_xscale +=0.1
}

