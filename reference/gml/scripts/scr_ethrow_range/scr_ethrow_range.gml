function scr_ethrow_range() {
	//type 1
	scr_things()
	if instance_exists(obj_player){
	if state = -1 and distance_to_object(obj_player) < range{
	state = 0
	}
	if state = 0{
	scr_ethrow0()
	image_index = 1
	direction = point_direction(x,y,obj_player.x,obj_player.y)
	}
	if state = 1 {
	scr_ethrow_atk()
	}
	}




}
