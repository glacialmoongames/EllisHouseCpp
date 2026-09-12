function scr_ethrow0() {
	// state 0
	if global.pause = 0{
	timer ++

	if timer < 40{
	y--
	}else if timer > 50{
	state = 1
	timer = 0
	}
	}



}
