function scr_wall_jump() {

	// wall jump

	// check if jump is possible
	if place_meeting(x, y + 1, obj_colision)
	{
	    vsp = jump_k * -jumpspeed;
	    grounded = true
	}else
	{
	    grounded = false
	}

	// jumping
	if (jump_k) and place_meeting(x + 1, y, obj_colision) and grounded = false and right_k and room != rm_boss
	{
	    vsp = -jumpspeed;
	    move = -1
	    move_lock = true
	    alarm[0] = 10 
	    instance_create(x,y,obj_wallsmoke)
	    audio_play_sound(sfx_jump,0,0)
    
	}
	if (jump_k) and place_meeting(x - 1, y, obj_colision) and grounded = false and -left_k and room != rm_boss 
	{
	    vsp = -jumpspeed;
	    move = 1
	    move_lock = true
	    alarm[0] = 10 
	    instance_create(x,y,obj_wallsmoke)
	    audio_play_sound(sfx_jump,0,0)
	}

	// slow in wall
	if place_meeting(x + 1, y, obj_colision)and grounded = false  and vsp>0 and right_k and room != rm_boss {
	 vsp = vsp /1.75
	}
	if place_meeting(x - 1, y, obj_colision)and grounded = false  and vsp>0 and -left_k and room != rm_boss {
	 vsp = vsp /1.75
	}




}
