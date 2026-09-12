/// @description  timer counter
if global.pause = 0{
if global.plus = 1{
if room != rm_end and room != rm_menu and room != rm_credits{
global.minute+= (delta_time*0.000001)*room_speed;
global.sec+= (delta_time*0.000001)*room_speed;
global.milisec+= (delta_time*0.000001)*room_speed;
if global.sec > 3600 {
    global.sec = 0
}
}
}
}

