draw_set_valign(fa_middle);
draw_set_halign(fa_middle);
draw_set_font(font0);

var m
for (m = 0; m < array_length_1d(menu); m += 1)
{
    draw_text_transformed_colour(x, y + (m * space*2),string_hash_to_newline(string(menu[m])),0.75,0.75,0,c_white,c_white,c_white,c_white,1)
}

draw_sprite_ext(spr_arrow,0,x-51,y+mpos*space*2,1,1,0,c_white,1)

draw_set_halign(fa_left);
draw_set_valign(fa_left);
if mpos*space*2 = 24{
draw_sprite_ext(spr_brazil,0,x+80,y+24,midi,midi,0,c_white,1)
}else draw_sprite_ext(spr_usa,0,x+80,y,midi,midi,0,c_white,1)

// pulsing
if mm = 0{
midi +=0.005
if midi = maxi{
mm = 1
}
}else if mm = 1{
midi -=0.005
if midi = mini{
mm = 0
}
}


