function scr_dash() {

	if dead = 0{
	instance_create(x,y,obj_trail)
	}
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

	// vertical colision
	if (place_meeting(x,y+vsp,obj_colision)) {
	    while(!place_meeting(x,y+sign(vsp),obj_colision)){
	        y += sign(vsp);
	    }
	    vsp = 0;
	}
	y += vsp;

	if dead = 0{
	if !(place_meeting(x+hsp*2.5,y,obj_colision)) {
	x+=hsp*2.5
	move_lock = true;
	}else state = scr_move() move_lock = false;
	}
	vsp = 0



}
