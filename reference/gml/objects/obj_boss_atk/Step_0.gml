if global.pause = 0{
image_speed = 0.2
timer ++ 
if obj_boss.state = 1{
if timer < 2{
instance_create(96,165,obj_trash_ball)
}else if timer = 15 {
    instance_create(96,165,obj_trash_ball)
}else if timer = 30{
    instance_create(96,165,obj_trash_ball)
}
}else if obj_boss.state = 3{
if timer < 2{
instance_create(90,160,obj_bosscar)
}else if timer = 60{
instance_create(90,160,obj_bosscar)
}
}
}else image_speed = 0


