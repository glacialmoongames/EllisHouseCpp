function scr_lang() {
	switch (mpos)
	{
	    case 0: 
	    {
	        room_goto_next()
	        global.language = 0
	        break;
	    }
	    case 1: 
	    {   
	        room_goto_next()
	        global.language = 1
	        break;
	    }
	}



}
