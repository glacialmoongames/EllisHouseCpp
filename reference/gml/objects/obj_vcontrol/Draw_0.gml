draw_set_colour(c_white)
// esquerda
draw_circle(__view_get( e__VW.XView, 0 )+24,__view_get( e__VW.YView, 0 )+200,16,0)
// direita
draw_circle(__view_get( e__VW.XView, 0 )+64,__view_get( e__VW.YView, 0 )+200,16,0)
// cima
draw_circle(__view_get( e__VW.XView, 0 )+364,__view_get( e__VW.YView, 0 )+148,12,0)
// baixo
draw_circle(__view_get( e__VW.XView, 0 )+364,__view_get( e__VW.YView, 0 )+174,12,0)
// pulo
draw_circle(__view_get( e__VW.XView, 0 )+364,__view_get( e__VW.YView, 0 )+200,12,0)


draw_sprite_ext(spr_pausa,0,__view_get( e__VW.XView, 0 )+370,__view_get( e__VW.YView, 0 )+16,1.5,1.5,0,c_white,1)

if (point_in_rectangle(mouse_x,mouse_y,(__view_get( e__VW.XView, 0 )+370)-8,(__view_get( e__VW.YView, 0 )+16)-8,(__view_get( e__VW.XView, 0 )+370)+8,(__view_get( e__VW.YView, 0 )+16)+8) and mouse_check_button_released(mb_left)and room != rm_language and room != rm_menu 
	and room != rm_boss_scene and room != rm_end and room != rm_credits and 
	!instance_exists(obj_fade) and instance_exists(obj_player)){
	    if obj_player.dead = 0{
	        if global.pause = 0{
	            global.pause = 1
	        } else global.pause = 0
	    }
	}