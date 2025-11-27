#include <stdint.h>

#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#include "aircraft_state.h"

#define TILE_RES 1200
#define CHUNK_RES 200
#define CHUNK_WORLD_SIZE (CHUNK_RES * 90)

#define CAMERA_MOUSE_SENS 0.003f
#define CAMERA_BASE_SPEED 100.0f
#define FAST_MULTIPLIER 100.0f
#define SLOW_MULTIPLIER 0.25f

Vector3 worldOffset = {0};

struct aircraft_state ac = {
    .speed = 200,
    .pos = {0, 0, 0},
    .forward = {0, 0, 1},
    .up = {0, 1, 0},
    .right = {-1, 0, 0},
};

void UpdateCameraAircraft(Camera3D *camera, struct aircraft_state *ac) {
    worldOffset = ac->pos;
    camera->target = ac->forward; // since camera is fixed at 0,0,0
    camera->up = ac->up;
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

    // Sky Shader
    Shader sky = LoadShader("shaders/sky.vs", "shaders/sky.fs");
    int locInvView = GetShaderLocation(sky, "invView");
    int locInvProj = GetShaderLocation(sky, "invProj");

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
    rlSetClipPlanes(1, 100000);
    bool freecam = 0;
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        if (IsKeyPressed(KEY_C)) {
            freecam = !freecam;
            if (freecam) {
                ac.forward = (Vector3){0, 0, 1};
                ac.up = (Vector3){0, 1, 0};
                ac.right = (Vector3){-1, 0, 0};
            }
        }
        if (freecam) {
            float speed = CAMERA_BASE_SPEED * GetFrameTime();

            if (IsKeyDown(KEY_LEFT_SHIFT))
                speed *= FAST_MULTIPLIER;

            if (IsKeyDown(KEY_W))
                ac.pos = Vector3Add(ac.pos, Vector3Scale(ac.forward, speed));
            if (IsKeyDown(KEY_S))
                ac.pos = Vector3Add(ac.pos, Vector3Scale(ac.forward, -speed));
            if (IsKeyDown(KEY_A))
                ac.pos = Vector3Add(ac.pos, Vector3Scale(ac.right, -speed));
            if (IsKeyDown(KEY_D))
                ac.pos = Vector3Add(ac.pos, Vector3Scale(ac.right, speed));

            if (IsKeyDown(KEY_SPACE))
                ac.pos.y += speed;
            if (IsKeyDown(KEY_LEFT_CONTROL))
                ac.pos.y -= speed;

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
        UpdateCameraAircraft(&camera, &ac);

        BeginDrawing();

        ClearBackground(BLACK);

        Matrix proj = GetCameraProjectionMatrix(&camera, CAMERA_FREE);
        Matrix view = GetCameraViewMatrix(&camera);

        Matrix invProj = MatrixInvert(proj);
        Matrix invView = MatrixInvert(view);

        SetShaderValueMatrix(sky, locInvProj, invProj);
        SetShaderValueMatrix(sky, locInvView, invView);
        BeginShaderMode(sky);
        rlDisableDepthTest();
        rlDrawVertexArray(0, 3);
        rlBegin(RL_TRIANGLES);
        rlVertex2f(-1.0f, -1.0f);
        rlVertex2f(3.0f, -1.0f);
        rlVertex2f(-1.0f, 3.0f);
        rlEnd();
        EndShaderMode();
        rlEnableDepthTest();

        SetShaderValue(shader, aircraftPosLoc, (float[3]){ac.pos.x, ac.pos.y, ac.pos.z},
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, aircraftForwardLoc,
                       (float[3]){ac.forward.x, ac.forward.y, ac.forward.z}, SHADER_UNIFORM_VEC3);

        BeginMode3D(camera);

        int cx = worldOffset.x / CHUNK_WORLD_SIZE;
        int cz = worldOffset.z / CHUNK_WORLD_SIZE;
        int viewDist = 5;
        for (int dz = -viewDist; dz <= viewDist; dz++) {
            for (int dx = -viewDist; dx <= viewDist; dx++) {
                int ccx = cx + dx;
                int ccz = cz + dz;

                float offset[2] = {(float)ccx, (float)ccz};
                SetShaderValue(shader, chunkOffsetLoc, offset, SHADER_UNIFORM_VEC2);

                Vector3 worldPos = {ccx * CHUNK_WORLD_SIZE, 0, ccz * CHUNK_WORLD_SIZE};

                // Apply world offset (camera stays at origin)
                worldPos = Vector3Subtract(worldPos, worldOffset);

                DrawModel(chunkModel, worldPos, 1.0f, WHITE);
            }
        }

        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("%.1f, %.1f, %.1f", worldOffset.x, worldOffset.y, worldOffset.z), 10,
                 40, 20, WHITE);

        EndDrawing();
    }

    UnloadTexture(dem_tex);

    CloseWindow();

    return 0;
}
