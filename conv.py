from PIL import Image
import numpy as np
import numpy as np
from scipy import ndimage

dem = np.array(Image.open("Copernicus_DSM_COG_30_N44_00_W074_00_DEM.tif"), dtype=np.int16)
dem.astype(np.float32).tofile("dem.raw")

wbm = np.array(Image.open("Copernicus_DSM_COG_30_N44_00_W074_00_WBM.tif"), dtype=np.int8)
solid = wbm > 0

# Distance to nearest empty pixel
dist_outside = ndimage.distance_transform_edt(solid)
# Distance to nearest solid pixel
dist_inside = ndimage.distance_transform_edt(~solid)

sdf = dist_inside - dist_outside
sdf_norm = sdf / wbm.shape[0]
sdf_norm.astype(np.float32).tofile("wbm_sdf.raw")
