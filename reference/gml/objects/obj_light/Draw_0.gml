draw_set_blend_mode(bm_subtract);
if surface_exists(light){
draw_surface(light,__view_get( e__VW.XView, 0 ),__view_get( e__VW.YView, 0 ));
}else light = surface_create(__view_get( e__VW.WView, 0 ),__view_get( e__VW.HView, 0 ))
draw_set_blend_mode(bm_normal);

