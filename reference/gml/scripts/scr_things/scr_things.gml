function scr_things() {

	if type = 0{
	depth = -1
	} else if type = 1{
	depth = -2
	} else if type = 2{
	depth = -1
	} else if type = 3{
	depth = 0
	} else if type = 4{
	depth = 1
	}
	if global.pause = 0 {
	if type != 4{
	if place_meeting(x,y,obj_player){
	    if obj_player.sprite_index != spr_player_blink and obj_player.sprite_index != spr_mib_blink and obj_player.sprite_index != spr_pope_blink{
	        if obj_player.dead != 1{
	        audio_play_sound(sfx_hurt,1,0)
	        }
	        obj_player.dead = 1
	    }
	}
	}
	if x < -100 or x > room_width+100 or y < -32 or y > room_height+32{
	    instance_destroy()
	}
	}



}
