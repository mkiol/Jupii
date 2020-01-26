import glob, os

dir_name = "icons"

app_name = "jupii"

# "dir_name": size
sizes = {
    "86x86": 86,
    "108x108": 108,
    "128x128": 128,
    "150x150": 150,
    "172x172": 172,
    "256x256": 256
}

svg = app_name + ".svg"

if not os.path.exists(dir_name):
    os.makedirs(dir_name)

for name in sizes.keys():
    d = dir_name + "/" + name
    
    if not os.path.exists(d):
        os.makedirs(d)
        
    png = d + "/harbour-" + app_name + ".png"
    
    if not os.path.isfile(png):
        size = sizes[name]
        print("File name: %s, size: %d" % (png, size))
        print("inkscape -f {} -e {} -C -w {:.0f} -h {:.0f}"
                  .format(svg, png, size, size))
        os.system("inkscape -f {} -e {} -C -w {:.0f} -h {:.0f}"
                  .format(svg, png, size, size))

