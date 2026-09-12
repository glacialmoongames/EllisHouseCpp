
image_alpha += 0.1
timer ++
if timer > 60{
    if instance_exists(obj_player){
        if obj_player.dead = 1{
            room_restart()
            global.count = global.old_count
            obj_player.x = obj_player.respawnx
            obj_player.y = obj_player.respawny
            obj_player.dead = 0
            timer = 0
        }
    }
}

if timer > 90{
    if !instance_exists(obj_player){
        global.old_count = global.count
        room_goto_next()
        instance_destroy()
        timer = 0
    }
}



