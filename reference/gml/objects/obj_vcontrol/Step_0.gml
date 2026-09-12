// reset
global.touch_left = 0;
global.touch_right = 0;
global.touch_up = 0;
global.touch_down = 0;

global.touch_jump = 0;
global.touch_jump_pressed = 0;


// =====================================
// MULTITOUCH
// =====================================

for (var i = 0; i < 5; i++)
{
    if (device_mouse_check_button(i, mb_left))
    {
        var tx = device_mouse_x(i);
        var ty = device_mouse_y(i);

        // LEFT
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+24,
            __view_get(e__VW.YView,0)+200
        ) <= 16)
        {
            global.touch_left = 1;
        }

        // RIGHT
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+64,
            __view_get(e__VW.YView,0)+200
        ) <= 16)
        {
            global.touch_right = 1;
        }

        // UP
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+364,
            __view_get(e__VW.YView,0)+148
        ) <= 12)
        {
            global.touch_up = 1;
        }

        // DOWN
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+364,
            __view_get(e__VW.YView,0)+174
        ) <= 12)
        {
            global.touch_down = 1;
        }

        // JUMP HOLD
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+364,
            __view_get(e__VW.YView,0)+200
        ) <= 12)
        {
            global.touch_jump = 1;
        }
    }


    // PRESS
    if (device_mouse_check_button_pressed(i, mb_left))
    {
        var tx = device_mouse_x(i);
        var ty = device_mouse_y(i);

        // JUMP PRESS
        if (point_distance(
            tx, ty,
            __view_get(e__VW.XView,0)+364,
            __view_get(e__VW.YView,0)+200
        ) <= 12)
        {
            global.touch_jump_pressed = 1;
        }
    }
}

if room == rm_menu or room == rm_credits or room == rm_end {
	instance_destroy()	
}