function scr_loadgame() {
	if (file_exists("Save.sav"))
	{
	    ini_open("Save.sav")
	    global.end_game = ini_read_real("Save","Game+",0)
	    global.besttime = ini_read_real("Save","Time",0)
	    global.bestsec = ini_read_real("Save","Second",0)
	    global.bestminute = ini_read_real("Save","Minute",0)
	    global.bestmilisec = ini_read_real("Save","Milisecond",0)
	    ini_close()
	}

	/*
	ini_write_real("Save","Time",global.besttime)
	ini_write_real("Save","Second",global.bestsec)
	ini_write_real("Save","Minute",global.bestminute)
	ini_write_real("Save","Milisecond",global.bestmilisec)
	*/



}
