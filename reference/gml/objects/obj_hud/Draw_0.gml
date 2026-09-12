/// @description  chocolate and death counter
draw_set_font(font0)
draw_set_color(c_white)
if global.pause = 0{
if instance_exists(obj_player){
    x = obj_player.x
    y = obj_player.y
    if obj_player.dead = 0 {
        draw_set_halign(fa_left)
        if global.plus = 0{
            draw_sprite_ext(spr_chocolat,0,__view_get( e__VW.XView, 0 )+15,__view_get( e__VW.YView, 0 )+15,1,1,0,c_white,1)
            draw_text_transformed(__view_get( e__VW.XView, 0 )+25,__view_get( e__VW.YView, 0 )+11,string_hash_to_newline(global.count),0.75,0.75,0)
        }
        draw_sprite_ext(spr_cavera,0,__view_get( e__VW.XView, 0 )+330,__view_get( e__VW.YView, 0 )+205,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+340,__view_get( e__VW.YView, 0 )+200,string_hash_to_newline(global.deathcount),0.75,0.75,0)
    // 690x400
}
}
}
draw_set_colour(c_black)
draw_set_halign(fa_center)



/// placas
draw_set_font(font0)
draw_set_colour(c_black)
draw_set_halign(fa_center)

if global.pause = 0{
if instance_exists(obj_player){
if obj_player.dead = 0and obj_player.hsp = 0{
    if instance_exists(obj_placa1) and obj_player.down_k and obj_placa1.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa1.text),0.5,0.5,0)
        
    }else if instance_exists(obj_placa2) and obj_player.down_k and obj_placa2.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa2.text),0.5,0.5,0)
        
    }else if instance_exists(obj_placa3) and obj_player.down_k and obj_placa3.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa3.text),0.5,0.5,0)
        
    }else if instance_exists(obj_placa4) and obj_player.down_k and obj_placa4.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa4.text),0.5,0.5,0)
        
    }else if instance_exists(obj_placa5) and obj_player.down_k and obj_placa5.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa5.text),0.5,0.5,0)
        
    }else if instance_exists(obj_placa6) and obj_player.down_k and obj_placa6.image_index = 1{
        draw_sprite_ext(spr_popup,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109,1,1,0,c_white,1)
        draw_text_transformed(__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+109-37,string_hash_to_newline(obj_placa6.text),0.5,0.5,0)
        }
}
}
}





/// timer 
draw_set_halign(fa_left)
if room != rm_menu and room != rm_credits{// colocar as dos finais +
watch += 0.25
if global.plus = 1 and global.pause = 0{
    if global.minute <25200{
        color = c_white
        draw_sprite(spr_stopwatch,watch,__view_get( e__VW.XView, 0 )+13+pos,__view_get( e__VW.YView, 0 )+13+pos/1.2)
    }else {draw_sprite(spr_stopwatch_red,watch*2,__view_get( e__VW.XView, 0 )+13+pos,__view_get( e__VW.YView, 0 )+13+pos/1.2) color = 6247869}

    if global.sec < 601{// sec < 10
        draw_text_transformed_colour(__view_get( e__VW.XView, 0 )+25+pos,__view_get( e__VW.YView, 0 )+10+pos/1.2,string_hash_to_newline(string(global.minute div 3600) + ":"+ "0" + string(global.sec div 60) 
        + ":" + string(round(global.milisec mod 60*1.0101010101))),0.5,0.5,0,color,color,color,color,1)
    }else { // sec > 9
        draw_text_transformed_colour(__view_get( e__VW.XView, 0 )+25+pos,__view_get( e__VW.YView, 0 )+10+pos/1.2,string_hash_to_newline(string(global.minute div 3600) + ":"+ string(global.sec div 60) 
        + ":" + string(round(global.milisec mod 60*1.0101010101))),0.5,0.5,0,color,color,color,color,1)
    }
}
}
draw_set_halign(fa_center)

///Timer pos
if room = rm_end and global.plus = 1{
timerpos++
if timerpos < 74{
pos +=2
} 
}else {pos = 0 timerpos = 0}

/// Hud boss life
if room = rm_boss and global.pause = 0{
draw_set_color(6247869)
if instance_exists(obj_player){
if obj_player.dead = 0{
    draw_sprite_ext(spr_bossbar_back,0,__view_get( e__VW.XView, 0 )+50,__view_get( e__VW.YView, 0 )+185,1,1,0,c_white,1)
    draw_rectangle(__view_get( e__VW.XView, 0 )+68,__view_get( e__VW.YView, 0 )+192,obj_boss.life,__view_get( e__VW.YView, 0 )+205,0)
    draw_sprite_ext(spr_bossbar,0,__view_get( e__VW.XView, 0 )+50,__view_get( e__VW.YView, 0 )+185,1,1,0,c_white,1)
}
}
}

