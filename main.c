#include <stdint.h>

#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#include "aircraft_state.h"

#define WGS84_A 6378137.0         // major axis
#define WGS84_E2 6.69437999014e-3 // eccentricity^2

#define TILE_RES 1200
#define TILE_WORLD_SIZE (111320 * cos(44.0 * DEG2RAD))
#define NUM_CHUNKS 6
#define CHUNK_RES (TILE_RES / NUM_CHUNKS)
#define CHUNK_WORLD_SIZE (TILE_WORLD_SIZE / NUM_CHUNKS)

#define CAMERA_MOUSE_SENS 0.003f
#define CAMERA_BASE_SPEED 100.0f
#define FAST_MULTIPLIER 100.0f

Vector3 worldOffset = {0};
bool freecam = true;

struct aircraft_state ac = {
    .speed = 200,
    .lat = 0,
    .lon = 0,
    .alt = 0,
    .forward = {0, 0, 1},
    .up = {0, 1, 0},
    .right = {-1, 0, 0},
};

Vector3 geodetic_to_ecef(double lat, double lon, double alt) {
    double sinLat = sin(lat);
    double cosLat = cos(lat);
    double sinLon = sin(lon);
    double cosLon = cos(lon);

    double N = WGS84_A / sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    Vector3 ecef;
    ecef.x = (N + alt) * cosLat * cosLon;
    ecef.y = (N + alt) * cosLat * sinLon;
    ecef.z = ((1.0 - WGS84_E2) * N + alt) * sinLat;

    return ecef;
}

Vector3 ecef_to_enu(Vector3 p, Vector3 origin, double lat0, double lon0) {
    double sinLat = sin(lat0), cosLat = cos(lat0);
    double sinLon = sin(lon0), cosLon = cos(lon0);

    // Translate to origin
    double dx = p.x - origin.x;
    double dy = p.y - origin.y;
    double dz = p.z - origin.z;

    Vector3 enu;

    // ENU conversion matrix
    enu.x = -sinLon * dx + cosLon * dy;                                 // east
    enu.y = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz; // north
    enu.z = cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;  // up

    return enu;
}

Vector3 geodetic_to_enu(double lat, double lon, double alt, double lat0, double lon0,
                        Vector3 originECEF) {

    Vector3 pECEF = geodetic_to_ecef(lat, lon, alt);
    return ecef_to_enu(pECEF, originECEF, lat0, lon0);
}

void update_camera_aircrat(Camera3D *camera, struct aircraft_state *ac) {
    double lat0 = 0.0;
    double lon0 = 0.0;
    Vector3 originECEF = geodetic_to_ecef(lat0, lon0, 0.0);
    Vector3 enu =
        geodetic_to_enu(ac->lat * DEG2RAD, ac->lon * DEG2RAD, ac->alt, lat0, lon0, originECEF);

    worldOffset.x = enu.x; // east
    worldOffset.y = enu.z; // up
    worldOffset.z = enu.y; // north

    camera->target = ac->forward; // since camera is fixed at 0,0,0
    camera->up = ac->up;
}

void handle_input() {
    if (IsKeyPressed(KEY_C)) {
        freecam = !freecam;
        if (freecam) {
            ac.up = (Vector3){0, 1, 0}; // reset the up vector so that in freecam, there is no roll
        }
    }
    if (freecam) {
        Vector3 move = {0};
        if (IsKeyDown(KEY_W))
            move.z += 1;
        if (IsKeyDown(KEY_S))
            move.z -= 1;
        if (IsKeyDown(KEY_D))
            move.x += 1;
        if (IsKeyDown(KEY_A))
            move.x -= 1;
        if (IsKeyDown(KEY_SPACE))
            move.y += 1;
        if (IsKeyDown(KEY_LEFT_CONTROL))
            move.y -= 1;

        if (Vector3Length(move) > 0.001)
            move = Vector3Normalize(move);

        Vector3 worldMove =
            Vector3Add(Vector3Scale(ac.forward, move.z),
                       Vector3Add(Vector3Scale(ac.right, move.x), Vector3Scale(ac.up, move.y)));

        float speed = CAMERA_BASE_SPEED;
        if (IsKeyDown(KEY_LEFT_SHIFT))
            speed *= FAST_MULTIPLIER;

        float ds = speed * GetFrameTime(); // m/s → meters per frame
        worldMove = Vector3Scale(worldMove, ds);

        // Convert meters → geodetic update
        ac.lat += worldMove.z / 111320.0;                           // north (ENU y)
        ac.lon += worldMove.x / (111320.0 * cos(ac.lat * DEG2RAD)); // east (ENU x)
        ac.alt += worldMove.y;                                      // up is ENU z

        Vector2 md = GetMouseDelta();
        // pitch
        float pitch_angle = -md.y * CAMERA_MOUSE_SENS;
        ac.forward = Vector3RotateByAxisAngle(ac.forward, ac.right, pitch_angle);
        ac.right = Vector3Normalize(Vector3CrossProduct(ac.forward, ac.up));
        // yaw
        float yaw_angle = -md.x * CAMERA_MOUSE_SENS;
        ac.forward = Vector3RotateByAxisAngle(ac.forward, ac.up, yaw_angle);
    } else {
        aircraft_update(&ac, GetFrameTime());
        if (IsKeyDown(KEY_W))
            aircraft_pitch(&ac, -0.01);
        if (IsKeyDown(KEY_S))
            aircraft_pitch(&ac, 0.01);
        if (IsKeyDown(KEY_A))
            aircraft_roll(&ac, -0.01);
        if (IsKeyDown(KEY_D))
            aircraft_roll(&ac, 0.01);
        if (IsKeyDown(KEY_Q))
            aircraft_yaw(&ac, 0.01);
        if (IsKeyDown(KEY_E))
            aircraft_yaw(&ac, -0.01);
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1024, 768, "synvis");
    DisableCursor();

    Camera camera = {0};
    camera.position = (Vector3){0, 0, 0};
    camera.target = (Vector3){1, 0, 0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Image dem_img = LoadImageRaw("dem.raw", TILE_RES, TILE_RES, RL_PIXELFORMAT_UNCOMPRESSED_R32, 0);
    Texture dem_tex = LoadTextureFromImage(dem_img);
    UnloadImage(dem_img);
    Image wbm_img =
        LoadImageRaw("wbm_sdf.raw", TILE_RES, TILE_RES, RL_PIXELFORMAT_UNCOMPRESSED_R32, 0);
    Texture wbm_tex = LoadTextureFromImage(wbm_img);
    UnloadImage(wbm_img);

    // Terrain shader
    Shader shader = LoadShader("shaders/terrain.vs", "shaders/terrain.fs");
    int heightmapLoc = GetShaderLocation(shader, "heightmap");
    rlEnableShader(shader.id);
    rlActiveTextureSlot(1);
    rlEnableTexture(dem_tex.id);
    rlSetUniformSampler(heightmapLoc, 1);
    int watermaskLoc = GetShaderLocation(shader, "watermask");
    rlEnableShader(shader.id);
    rlActiveTextureSlot(2);
    rlEnableTexture(wbm_tex.id);
    rlSetUniformSampler(watermaskLoc, 2);

    // Create a plane mesh and model
    Mesh mesh = GenMeshPlane(CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE, CHUNK_RES, CHUNK_RES);
    Model chunkModel = LoadModelFromMesh(mesh);
    chunkModel.materials[0].shader = shader;

    int chunkOffsetLoc = GetShaderLocation(shader, "chunkOffset");
    int heightmapSizeLoc = GetShaderLocation(shader, "heightmapSize");
    int chunkResLoc = GetShaderLocation(shader, "chunkRes");
    int chunkSizeLoc = GetShaderLocation(shader, "chunkSize");
    int lightDirLoc = GetShaderLocation(shader, "lightDir");
    int aircraftPosLoc = GetShaderLocation(shader, "aircraftPos");
    int aircraftForwardLoc = GetShaderLocation(shader, "aircraftForward");
    SetShaderValue(shader, heightmapSizeLoc, (float[1]){(float)TILE_RES}, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, chunkResLoc, (float[1]){(float)CHUNK_RES}, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, chunkSizeLoc, (float[1]){(float)CHUNK_WORLD_SIZE}, SHADER_UNIFORM_FLOAT);

    Vector3 lightDir = {0.5, 1.0, 0.5}; // TODO: sync with real life sun
    SetShaderValue(shader, lightDirLoc, (float[3]){lightDir.x, lightDir.y, lightDir.z},
                   SHADER_UNIFORM_VEC3);

    SetTargetFPS(60);
    rlDisableBackfaceCulling();
    rlSetClipPlanes(1, 1000000);
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        handle_input();
        update_camera_aircrat(&camera, &ac);

        BeginDrawing();

        ClearBackground((Color){28, 57, 195});

        BeginMode3D(camera);

        int tileMinX = 0;
        int tileMinZ = 0;
        int tileMaxX = NUM_CHUNKS - 1;
        int tileMaxZ = NUM_CHUNKS - 1;
        int tx = worldOffset.x / CHUNK_WORLD_SIZE;
        int tz = worldOffset.z / CHUNK_WORLD_SIZE;

        int viewDist = 5;
        for (int dz = -viewDist; dz <= viewDist; dz++) {
            for (int dx = -viewDist; dx <= viewDist; dx++) {
                int cx = tx + dx;
                int cz = tz + dz;

                if (cx < tileMinX || cx > tileMaxX || cz < tileMinZ || cz > tileMaxZ)
                    continue;

                float offset[2] = {(float)cx, (float)cz};
                SetShaderValue(shader, chunkOffsetLoc, offset, SHADER_UNIFORM_VEC2);

                Vector3 worldPos = {cx * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f, 0,
                                    cz * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f};

                worldPos = Vector3Subtract(worldPos, worldOffset);

                DrawModel(chunkModel, worldPos, 1.0f, WHITE);
            }
        }

        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("lat=%f", ac.lat), 10, 40, 20, WHITE);
        DrawText(TextFormat("lon=%f", ac.lon), 10, 70, 20, WHITE);
        DrawText(TextFormat("alt=%f", ac.alt), 10, 100, 20, WHITE);
        DrawText(TextFormat("%.1f, %.1f, %.1f", worldOffset.x, worldOffset.y, worldOffset.z), 10,
                 130, 20, WHITE);

        EndDrawing();
    }

    UnloadTexture(dem_tex);

    CloseWindow();

    return 0;
}
