function scr_pause() {
	switch (mpos)
	{
	    case 0: 
	    {   
	        global.pause = 0
	        break;
	    }
	    case 1: 
	    {   
	        obj_player.x = 80
	        obj_player.y = 144
	        global.pause = 0
	        room = rm_0
	        global.sec = 0
	        global.milisec = 0
	        global.minute = 0
	        if instance_exists(obj_control){
	            global.have_slide = false
	            global.have_dash = false
	            global.have_slow = false
	            global.count = 0
	            global.old_count = 0
	            global.deathcount = 0
	        }
	        break;
	    }
	    case 2: 
	    {
	        if window_get_fullscreen() = 0{
	        window_set_fullscreen(1);
	        }else{window_set_fullscreen(0);}
	        break;
	    }
	    case 3: 
	    {   
	        global.pause = 0
	        room = rm_menu
	        break;
	    }
	    case 4: 
	    {   
	        if global.mute = 0{
	        global.mute = 1
	        }else global.mute = 0
	        break;
	    }
	}



}
