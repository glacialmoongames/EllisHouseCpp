if global.pause = 0 {
if(instance_exists(obj_player))
{
    if (round(obj_player.y + (obj_player.sprite_height/2)) > y) or (obj_player.down_k) {mask_index = -1 ;}
    else mask_index = spr_platform;
}
}

