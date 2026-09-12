function scr_efollow_atk() {

	if instance_exists(obj_player){
	direction = point_direction(x,y,obj_player.x,obj_player.y)
	speed = 2
	if distance_to_object(obj_player) > range{
	state = 0
	image_index = 0
	speed = 0
	}
	}




}
