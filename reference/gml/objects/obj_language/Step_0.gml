var move = 0;

// teclado/gamepad
move -= max(
    keyboard_check_pressed(vk_up),
    keyboard_check_pressed(ord("W")),
    (gamepad_axis_value(0,gp_axislv) < 0) || gamepad_button_check_pressed(0,gp_padu),
0);

move += max(
    keyboard_check_pressed(vk_down),
    keyboard_check_pressed(ord("S")),
    (gamepad_axis_value(0,gp_axislv) > 0) || gamepad_button_check_pressed(0,gp_padd),
0);


// movimentação menu
if (move != 0)
{
    mpos += move;

    if (mpos < 0)
    {
        mpos = array_length_1d(menu) - 1;
    }

    if (mpos > array_length_1d(menu) - 1)
    {
        mpos = 0;
    }
}


// TOUCH
if (device_mouse_check_button_pressed(0, mb_left))
{
    var tx = mouse_x
    var ty = mouse_y

    // metade superior = English
    if (point_in_rectangle(tx, ty, 0, 0, 384, 109))
    {
        mpos = 0;
        scr_lang();
    }

    // metade inferior = PT-BR
    if (point_in_rectangle(tx, ty, 0, 109, 384, 218))
    {
        mpos = 1;
        scr_lang();
    }
}


// confirmar teclado/gamepad
var push = max(
    keyboard_check_pressed(vk_enter),
    keyboard_check_pressed(vk_shift),
    keyboard_check_pressed(vk_space),

    gamepad_button_check_pressed(0,gp_face1),
    gamepad_button_check_pressed(0,gp_face2),
    gamepad_button_check_pressed(0,gp_face3),
    gamepad_button_check_pressed(0,gp_face4),
    gamepad_button_check_pressed(0,gp_start),

0);

if (push == 1)
{
    scr_lang();
}


// reset globals
if (instance_exists(obj_control))
{
    global.have_slide = false;
    global.have_dash = false;
    global.have_slow = false;

    global.count = 0;
    global.old_count = 0;
    global.deathcount = 0;
}