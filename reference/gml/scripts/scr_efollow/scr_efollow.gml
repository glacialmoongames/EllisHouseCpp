function scr_efollow() {
	//type 2
	scr_things()
	if global.pause = 0 {

	if distance_to_object(obj_player) < range and state = 0 {
	    state = 1
	}

	if state = 1{
	    scr_shake()
	    scr_efollow0()
	}
	if state = 2{
	    image_index = 1
	    scr_shake()
	    scr_efollow_atk()
	}
	}



}
