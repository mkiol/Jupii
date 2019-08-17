import glob, os

dir_name = "images"

name = "upnp"

sizes = {16, 32, 48, 120, 256}

svg = name + ".svg"

if not os.path.exists(dir_name):
    os.makedirs(dir_name)

for size in sizes:
    png = "%s/upnp-%d.png" % (dir_name, size)
    jpg = "%s/upnp-%d.jpg" % (dir_name, size)
    
    if not os.path.isfile(png):
        cmd = "convert -density 1200 -resize %sx%s %s %s" % (size, size, svg, png)
        print(cmd)
        os.system(cmd)
        
    if not os.path.isfile(jpg):
        cmd = "convert -density 1200 -resize %sx%s %s %s" % (size, size, svg, jpg)
        print(cmd)
        os.system(cmd)
