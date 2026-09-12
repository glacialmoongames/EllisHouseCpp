function scr_eslide_boss() {
	// type 0
	scr_things()

	if distance_to_object(obj_player) < range and dir = 0{
	dir = 1
	state = 1
	}

	if state = 1{
	scr_eslide_atk()
	image_index = 1
	}



}
