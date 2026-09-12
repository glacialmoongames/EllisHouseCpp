function scr_plus_itens() {
	if global.pause = 0{
	timer ++
	if state = 0{
	    if timer > 60 {
	        image_speed = 0.2
	    }
	    scr_shake()
	} 

	if place_meeting(x,y,obj_player)and state = 0{
	    timer = 0
	    image_speed = 0
	    audio_play_sound(sfx_coin,1,0)
	    state = 1
	}
	if state = 1 {
	    if !instance_exists(obj_sparkle){
	        instance_create(x,y,obj_sparkle)
	    }
	    y --
	    image_alpha -=0.07
	    if image_alpha < 0.1 {
	        global.plus_itens ++
	        instance_destroy()
	    }
	}
	}else image_speed = 0
	if global.plus = 0{
	instance_destroy()
	}



}
