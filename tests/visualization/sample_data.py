import os
import csv

def write_rank_layout(base_dir, tiles):
    path = os.path.join(base_dir, "rank_layout.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["rank","x_off","y_off","nx","ny","halo","nxg","nyg"])
        for t in tiles:
            w.writerow([t["rank"], t["x_off"], t["y_off"], t["nx"], t["ny"],
                        t["halo"], t["nxg"], t["nyg"]])

def make_tiles_nx_ny(nxg, nyg, halo=1):
    nx0 = nxg // 2
    nx1 = nxg - nx0
    ny0 = nyg // 2
    ny1 = nyg - ny0
    return [
        {"rank":0,"x_off":0,   "y_off":0,   "nx":nx0,"ny":ny0,"halo":halo,"nxg":nxg,"nyg":nyg},
        {"rank":1,"x_off":nx0,"y_off":0,   "nx":nx1,"ny":ny0,"halo":halo,"nxg":nxg,"nyg":nyg},
        {"rank":2,"x_off":0,   "y_off":ny0,"nx":nx0,"ny":ny1,"halo":halo,"nxg":nxg,"nyg":nyg},
        {"rank":3,"x_off":nx0,"y_off":ny0,"nx":nx1,"ny":ny1,"halo":halo,"nxg":nxg,"nyg":nyg},
    ]
