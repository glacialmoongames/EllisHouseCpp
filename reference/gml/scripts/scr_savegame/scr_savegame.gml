function scr_savegame() {
	if (file_exists("Save.sav")) file_delete("Save.sav");
	ini_open("Save.sav");
	ini_write_real("Save","Game+",global.end_game)
	ini_write_real("Save","Time",global.besttime)
	ini_write_real("Save","Second",global.bestsec)
	ini_write_real("Save","Minute",global.bestminute)
	ini_write_real("Save","Milisecond",global.bestmilisec)
	ini_close()



}
