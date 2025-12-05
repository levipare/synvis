import subprocess
import os

DATA_DIR = "data"

tiles: list[tuple[int, int]] = [
    (43, -73),
    (44, -73),
    (43, -74),
    (44, -74),
]

lat_min, lat_max = 40, 50
lon_min, lon_max = -80, -70

tiles = [
    (lat, lon)
    for lat in range(lat_min, lat_max + 1)
    for lon in range(lon_min, lon_max + 1)
]


# example
# aws s3 cp --no-sign-request s3://copernicus-dem-90m/Copernicus_DSM_COG_30_N44_00_W073_00_DEM/Copernicus_DSM_COG_30_N44_00_W073_00_DEM.tif ./data
# aws s3 cp --no-sign-request s3://copernicus-dem-90m/Copernicus_DSM_COG_30_N44_00_W073_00_DEM/AUXFILES/Copernicus_DSM_COG_30_N44_00_W073_00_WBM.tif ./data
for lat, lon in tiles:
    dem_file = f"Copernicus_DSM_COG_30_{'N' if lat > 0 else 'S'}{lat:02d}_00_{'E' if lon > 0 else 'W'}{abs(lon):03d}_00_DEM.tif"
    wbm_file = f"Copernicus_DSM_COG_30_{'N' if lat >0 else 'S'}{lat:02d}_00_{'E' if lon > 0 else 'W'}{abs(lon):03d}_00_WBM.tif"

    s3_dem = f"s3://copernicus-dem-90m/Copernicus_DSM_COG_30_N{lat:02d}_00_W{abs(lon):03d}_00_DEM/{dem_file}"
    s3_wbm = f"s3://copernicus-dem-90m/Copernicus_DSM_COG_30_N{lat:02d}_00_W{abs(lon):03d}_00_DEM/AUXFILES/{wbm_file}"

    if dem_file not in os.listdir(DATA_DIR):
        subprocess.run(["aws", "s3", "cp", "--no-sign-request", s3_dem, DATA_DIR])

    if wbm_file not in os.listdir(DATA_DIR):
        subprocess.run(["aws", "s3", "cp", "--no-sign-request", s3_wbm, DATA_DIR])
