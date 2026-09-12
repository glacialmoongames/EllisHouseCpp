function scr_efall() {
	//type 4
	scr_things()

	if state != 1{
	scr_shake()
	}

	if state = 0 and distance_to_object(obj_player) < range{
	state = 1
	}
	if state = 1{
	image_index = 1
	scr_efall_atk()
	}





}
