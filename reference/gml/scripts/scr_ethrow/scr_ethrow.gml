function scr_ethrow() {
	//type 1
	scr_things()

	if state = 0{
	if sprite_index != spr_fireball{
	scr_ethrow0()
	}else state = 1
	direction = point_direction(x,y,obj_player.x,obj_player.y)
	}
	if state = 1 {
	scr_ethrow_atk()
	}




}
