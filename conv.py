from PIL import Image
import numpy as np

dem = np.array(Image.open("Copernicus_DSM_COG_30_N44_00_W074_00_DEM.tif"), dtype=np.int16)
wbm = np.array(Image.open("Copernicus_DSM_COG_30_N44_00_W074_00_WBM.tif"), dtype=np.int8)

water_bit = (wbm != 0).astype(np.uint16) & 0x1

# 15 bits DEM + 1 bit water mask
# store water in LSB
packed = (dem << 1) | water_bit

packed.astype(np.int16).tofile("terrain.raw")

