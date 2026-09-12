function scr_blink() {
	if blink != 0{
	blink --
	}
	if (vsp < 10) {
	    vsp += grav;
	}

	if place_meeting(x,y,obj_evil)and !instance_exists(obj_blink)and global.have_blink =1{
	    instance_create(x,y,obj_blink)
	    audio_play_sound(sfx_blink,0,0)
	}
	// jump
	if (place_meeting(x,y+1,obj_colision))
	{on_ground = true;}
	else
	{on_ground = false;}

	// coyote time

	if on_ground = false
	{
	    if coyote_counter > 0
	    {
	        coyote_counter -= 1;
        
	        if jumped = false
	        {
	            if (jump_k) 
	            {
	            vsp =-jumpspeed 
	            instance_create(x,y,obj_jumpsmoke)
	            audio_play_sound(sfx_jump,0,0)
	            jumped = true;
	            }
	        }
	    }
	}
	else
	{
	    jumped = false;
	    coyote_counter = coyote_max;
	}

	// Jump buffer

	if jump_k 
	{buffer_counter = buffer_max;}

	if buffer_counter > 0
	{
	    buffer_counter -=1;
    
	    if on_ground
	    {
	        vsp =-jumpspeed
	        buffer_counter = 0
	        instance_create(x,y,obj_jumpsmoke)
	        audio_play_sound(sfx_jump,0,0)
	        jumped = true;
	    }
	}
	// control height

	if (vsp < 0) and (!jumpheld_k) vsp = max(vsp,0)


	// horizontal colision
	if (place_meeting(x+hsp,y,obj_colision)) 
	{
	    yplus = 0;
	    while (place_meeting(x+hsp,y-yplus,obj_colision) and yplus <=abs(1*hsp)) yplus +=1;
	    if place_meeting(x+hsp,y-yplus,obj_colision)
	    {
    
	    while(!place_meeting(x+sign(hsp),y,obj_colision)){
	        x += sign(hsp);
	    }
	    hsp = 0;
	    }
	    else
	    {
	        y-=yplus;
	    }
	}
	x += hsp;

	// vertical colision
	if (place_meeting(x,y+vsp,obj_colision)) {
	    while(!place_meeting(x,y+sign(vsp),obj_colision)){
	        y += sign(vsp);
	    }
	    vsp = 0;
	}
	y += vsp;




}
