function scr_espawner() {
	scr_things()
	if global.pause = 0 {
	timer ++
	image_speed = 0
	if distance_to_object(obj_player) < range{
	image_index = 1
	if timer > spawnrate{
	instance_create(x,y,obj)
	timer = 0
	squishy = 1
	}
	}else image_index = 0
	}



}
