size = 48;
draw_set_blend_mode(bm_subtract);
surface_set_target(light);
draw_ellipse_colour(x-size/2 -__view_get( e__VW.XView, 0 ),y-size/2 -__view_get( e__VW.YView, 0 ),x+size/2-__view_get( e__VW.XView, 0 ),y+size/2-__view_get( e__VW.YView, 0 ),c_white,c_black,0)
surface_reset_target();
draw_set_blend_mode(bm_normal);

