scr_efollow()
if global.pause = 1{
    speed = 0
}
if image_xscale = -1{
    dir = 1
}else if image_xscale = 1{
    dir = 0
}
if state = 2{
if dir = 1{
image_xscale += 0.05
} else image_xscale -= 0.05
if dir = 1 and image_xscale = 0{
sprite_index = og_sprite
}else if dir = 0 and image_xscale = 0{
sprite_index = spr_paint_back
}
}else if state != 2{
    if image_xscale !=1 {
        if image_xscale = 0{
            sprite_index = og_sprite
        }
    image_xscale += 0.05
    }
}

