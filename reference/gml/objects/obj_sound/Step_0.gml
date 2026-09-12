/// @description Play Music
if global.mute = 0{
if room = rm_menu{
audio_stop_sound(snd_01)
audio_stop_sound(snd_02)
audio_stop_sound(snd_03)
audio_stop_sound(snd_04)
audio_stop_sound(snd_05)
audio_stop_sound(snd_boss)
if !audio_is_playing(snd_end)and !audio_is_playing(snd_menu){
audio_play_sound(snd_menu,1,1)
}
}
if room = rm_0{
audio_stop_sound(snd_menu)
audio_stop_sound(snd_end)
audio_stop_sound(snd_02)
audio_stop_sound(snd_03)
audio_stop_sound(snd_04)
audio_stop_sound(snd_05)
audio_stop_sound(snd_boss)
if !audio_is_playing(snd_01){
audio_play_sound(snd_01,1,1,0.005)
}
}
if room = rm_1{
audio_stop_sound(snd_01)
if !audio_is_playing(snd_02){
	audio_play_sound(snd_02,1,1,0.005)
}
}
if room = rm_2{
audio_stop_sound(snd_02)
if !audio_is_playing(snd_03){
audio_play_sound(snd_03,1,1,0.005)
}
}
if room = rm_3{
audio_stop_sound(snd_03)
if !audio_is_playing(snd_04){
audio_play_sound(snd_04,1,1,0.005)
}
}
if room = rm_4{
audio_stop_sound(snd_04)
if !audio_is_playing(snd_05){
audio_play_sound(snd_05,1,1,0.005)
}
}
if room = rm_5{
audio_stop_sound(snd_05)
if !audio_is_playing(snd_06){
audio_play_sound(snd_06,1,1,0.005)
}
}
if room = rm_boss{
audio_stop_sound(snd_05)
if !audio_is_playing(snd_boss){
audio_play_sound(snd_boss,1,1,0.005)
}
}
if room = rm_end{
audio_stop_sound(snd_boss)
if !audio_is_playing(snd_end){
audio_play_sound(snd_end,1,1,0.005)
}
}
}else audio_stop_all()

/// Pause
if global.pause = 1{
audio_pause_all()
}else audio_resume_all()

