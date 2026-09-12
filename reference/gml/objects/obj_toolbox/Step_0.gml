if timer = 0{
randomize()
obj = obj_tools
}
scr_espawner()

if squishy = 1{
    stime --
    image_xscale -=0.1
    if stime = 0{
        squishy = 0
        stime = 5
    }
}
if squishy = 0 and image_xscale !=1{
    image_xscale +=0.1
}

