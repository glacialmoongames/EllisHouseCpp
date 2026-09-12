draw_set_valign(fa_middle);
draw_set_halign(fa_middle);
draw_set_font(font0);

var m
for (m = 0; m < array_length_1d(menu); m += 1)
{
    draw_text_transformed_colour(x, y + (m * space*2),string_hash_to_newline(string(menu[m])),0.75,0.75,0,c_white,c_white,c_white,c_white,1)
}
//draw best time
if global.end_game = 1{
if !instance_exists(obj_bestlight){
instance_create(__view_get( e__VW.XView, 0 )+57,__view_get( e__VW.YView, 0 )+129,obj_bestlight)
}
draw_text_transformed_colour(__view_get( e__VW.XView, 0 )+57,__view_get( e__VW.YView, 0 )+129,string_hash_to_newline(bt),0.5,0.5,17,c_white,c_white,c_white,c_white,1)
if global.bestsec < 601{// sec < 10
        draw_text_transformed_colour(__view_get( e__VW.XView, 0 )+60,__view_get( e__VW.YView, 0 )+140,string_hash_to_newline(string(global.bestminute div 3600) + ":"+ "0" + string(global.bestsec div 60) 
        + ":" + string(round(global.bestmilisec mod 60*1.66666666667))),0.5+shake,0.5+shake,17,c_white,c_white,c_white,c_white,1)
    }else { // sec > 9
        draw_text_transformed_colour(__view_get( e__VW.XView, 0 )+60,__view_get( e__VW.YView, 0 )+140,string_hash_to_newline(string(global.bestminute div 3600) + ":"+ string(global.bestsec div 60) 
        + ":" + string(round(global.bestmilisec mod 60*1.66666666667))),0.5+shake,0.5+shake,17,c_white,c_white,c_white,c_white,1)
    }
}
draw_sprite_ext(spr_arrow,0,x-51,y+mpos*space*2,1,1,0,c_white,1)
draw_text_transformed_colour(room_width-15,room_height-5,string_hash_to_newline("v1.2"),0.25,0.25,0,c_white,c_white,c_white,c_white,1)
draw_set_halign(fa_left);
draw_set_valign(fa_left);

if global.end_game = 0{
    draw_sprite(spr_cross,0,room_width/2,room_height/2+42)
}

//Create shake best time
if recoi =0{
shake += angle_add
}
if shake >maxi //Max
{
recoi = 1
}
if recoi = 1{
shake -= angle_add
} 
if shake <mini //Min 
{
recoi = 0
}



