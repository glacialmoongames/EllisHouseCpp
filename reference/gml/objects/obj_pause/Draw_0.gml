draw_set_valign(fa_middle);
draw_set_halign(fa_middle);
draw_set_font(font0);
//Mute
if global.mute = 0 {
if dir = 0{
alter += 0.005
}else {alter -= 0.005}

if alter > 0.1{
dir = 1
} else if alter < 0{
dir = 0
}
}else alter = 0
// draw background
draw_sprite_ext(spr_fade,0,__view_get( e__VW.XView, 0 )+192,__view_get( e__VW.YView, 0 )+108,2,2,0,c_white,0.75)
draw_sprite_ext(spr_pause,global.language,__view_get( e__VW.XView, 0 )+190,__view_get( e__VW.YView, 0 )+110,1,1,0,c_white,1)
var m
for (m = 0; m < array_length_1d(menu); m += 1)
{
    draw_sprite_ext(spr_sound,global.mute,x,y + 95,1+alter,1+alter,0,c_white,1)
    draw_text_transformed_colour(x, y + (m * space*2),string_hash_to_newline(string(menu[m])),0.75,0.75,0,c_white,c_white,c_white,c_white,1)
}

draw_sprite_ext(spr_longarrow,0,x-51-12,y+mpos*space*2,1,1,0,c_white,1)

draw_set_halign(fa_left);
draw_set_valign(fa_left);



