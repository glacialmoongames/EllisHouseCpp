function scr_menu() {
	switch (mpos)
	{
	    case 0: 
	    {
	        room = rm_0
	        global.plus = 0
	        break;
	    }
	    case 1: 
	    {   
	        if global.end_game = 1 {
	        global.sec = 0
	        global.minute = 0
	        global.milisec = 0
	        global.time = 0
	        global.plus = 1
	        randomize()
	        global.skin = choose(0,1)
	        room = rm_0
        
	        }
	        break;
	    }
	    case 2: 
	    {
	        room = rm_credits
	        break;
	    }
	    case 3: 
	    {
	        game_end(); 
	        break;
	    }
	}



}
