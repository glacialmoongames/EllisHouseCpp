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
    // GUI SCALE x4
    var tx = mouse_x
    var ty = mouse_y

    // tamanho clicável dos botões
    var bw = 180;
    var bh = 20;

    // checa cada item do menu
    for (var i = 0; i < array_length_1d(menu); i++)
    {
        var bx1 = x - bw/2;
        var by1 = (y + (i * space * 2)) - bh/2;

        var bx2 = x + bw/2;
        var by2 = (y + (i * space * 2)) + bh/2;

        if (point_in_rectangle(tx, ty, bx1, by1, bx2, by2))
        {
            mpos = i;
            scr_menu();
        }
    }
}


// confirmar teclado/gamepad
var push = max(
    keyboard_check_pressed(vk_enter),
    keyboard_check_pressed(vk_space),

    gamepad_button_check_pressed(0,gp_face1),
    gamepad_button_check_pressed(0,gp_face2),
    gamepad_button_check_pressed(0,gp_face3),
    gamepad_button_check_pressed(0,gp_face4),
    gamepad_button_check_pressed(0,gp_start),

0);

if (push == 1)
{
    scr_menu();
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

    global.mute = 0;
}

if (instance_exists(obj_hud))
{
    obj_hud.m = 0;
    obj_hud.s = 0;
    obj_hud.timer = 0;
}