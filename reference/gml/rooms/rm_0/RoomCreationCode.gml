if !instance_exists(obj_player){
    instance_create(80,144,obj_player)
}
obj_player.respawnx = 80
obj_player.respawny = 144
if !instance_exists(obj_control){
    instance_create(0,0,obj_control)
}