from PIL import Image
import numpy as np
import re
import os

DATA_DIR = "data"


def extract_lon_lat(filename: str) -> tuple[int, int] | None:
    pattern = r"_(?P<lat_sign>[NS])(?P<lat_deg>\d{2})_00_(?P<lon_sign>[EW])(?P<lon_deg>\d{3})_00_DEM"

    match = re.search(pattern, filename)

    if match:
        data = match.groupdict()

        lat_deg = int(data["lat_deg"])
        lon_deg = int(data["lon_deg"])
        lat_sign = data["lat_sign"]
        lon_sign = data["lon_sign"]

        if lat_sign == "S":
            lat_deg *= -1

        if lon_sign == "W":
            lon_deg *= -1

        return lat_deg, lon_deg


def make_raw(fname: str):
    dem = np.array(Image.open(DATA_DIR + "/" + fname), dtype=np.int16)
    wbm = np.array(
        Image.open(DATA_DIR + "/" + fname.replace("DEM", "WBM")), dtype=np.int8
    )

    water_bit = (wbm != 0).astype(np.uint16) & 0x1

    # 15 bits DEM + 1 bit water mask
    # store water in LSB
    packed = (dem << 1) | water_bit

    return packed.astype(np.int16)


tdb = np.array([], dtype=np.int16)
for f in os.listdir(DATA_DIR):
    res = extract_lon_lat(f)
    if not res:
        continue
    lat, lon = res
    data = make_raw(f)
    x, y = data.shape
    tdb = np.append(tdb, [lat, lon, x, y])
    tdb = np.append(tdb, data.flatten())

tdb.astype(np.int16).tofile("terrain.tdb")
