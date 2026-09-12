if global.pause = 0{
if global.language = 1{
if room = rm_0{
    sprite_index = spr_title_bedroom
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_1{
    sprite_index = spr_title_stairs
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_2{
    sprite_index = spr_title_kitchen
    x = 96
    y = 400
}else if room = rm_3{
    sprite_index = spr_title_living
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_4{
    sprite_index = spr_title_corridor
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_5{
    sprite_index = spr_title_garage
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}
}else{
if room = rm_0{
    sprite_index = spr_en_bedroom
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_1{
    sprite_index = spr_en_stairs
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_2{
    sprite_index = spr_en_kitchen
    x = 96
    y = 400
}else if room = rm_3{
    sprite_index = spr_en_living
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_4{
    sprite_index = spr_en_corridor
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}else if room = rm_5{
    sprite_index = spr_en_garage
    x = __view_get( e__VW.WView, 0 )/2
    y = __view_get( e__VW.HView, 0 )/2
}
}
image_alpha -=0.02
if image_alpha < 0.01{
instance_destroy()
}
}

