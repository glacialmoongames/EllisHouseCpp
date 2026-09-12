draw_set_halign(fa_center)
draw_set_colour(c_white)
if global.language = 1{
// portuguese
draw_text_transformed(room_width/2,room_height/2,
string_hash_to_newline(@"Programado por Annie

Gráficos por Pavão Gripado e IGustaMe

Músicas por BainoLOL

Obrigado aos que testaram o jogo
pelo feedback e obrigado a você
por jogar!"),0.5,0.5,0)
if !gamepad_is_connected(0){
    draw_sprite_ext(spr_esc,0,10,212,0.5,0.5,0,c_white,1)
}else draw_sprite_ext(spr_b,0,10,212,0.5,0.5,0,c_white,1)
draw_text_transformed(55,210,string_hash_to_newline("para voltar ao menu"),0.25,0.25,0)
}
else
{// english
draw_text_transformed(room_width/2,room_height/2,
string_hash_to_newline(@"Programmed by Annie

Graphics by Pavão Gripado and IGustaMe

Musics by BainoLOL


Thanks to those who tested the game
for the feedback and thanks to you
for playing!"),0.5,0.5,0)
if !gamepad_is_connected(0){
    draw_sprite_ext(spr_esc,0,10,212,0.5,0.5,0,c_white,1)
}else draw_sprite_ext(spr_b,0,10,212,0.5,0.5,0,c_white,1)
draw_text_transformed(55,210,string_hash_to_newline("to return to menu"),0.25,0.25,0)
}
draw_set_halign(fa_left)
draw_set_colour(c_black)

if keyboard_check_pressed(vk_escape)or (gamepad_button_check_pressed(0,gp_face2) or mouse_check_button_pressed(mb_left)){
    room = rm_menu
}

