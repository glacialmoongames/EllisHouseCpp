draw_self()
if global.pause = 0{
if instance_exists(obj_player){
if place_meeting(x,y,obj_player){
image_index = 1
}else image_index = 0
if image_index = 1 and !obj_player.down_k{
al+=0.1
draw_sprite_ext(spr_button,0,obj_player.x,obj_player.y-25,1,1,0,c_white,al)
draw_text_transformed(obj_player.x+.5,obj_player.y-28.5,string_hash_to_newline("S"),0.5,0.5,0)
}else al = 0 
}}

