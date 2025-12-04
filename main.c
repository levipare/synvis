#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cglm/struct.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_SDL3
#define CIMGUI_USE_OPENGL3
#include "cimgui.h"
#include "cimgui_impl.h"

#include "aircraft_state.h"

struct mesh {
    GLuint vao, vbo, ebo;
    GLsizei index_count;
};

struct {
    struct mesh mesh;
    GLuint shader;
    GLuint dem_tex;
} terrain_model;

static SDL_Window *window;
static SDL_GLContext glctx;
static SDL_Joystick *joy;
static Uint64 last_tick;
static float mouse_dx, mouse_dy;

static bool freecam = true;
static vec3s world_offset = {0, 0, 0};
static struct aircraft_state ac = {
    .throttle = 0,
    .lat = 0,
    .lon = 0,
    .height = 0,
    .forward = {0, 0, 1},
    .up = {0, 1, 0},
    .right = {-1, 0, 0},
};

#define TILE_RES 1200
#define TILE_WORLD_SIZE (111320 * cos(glm_rad(44)))
#define NUM_CHUNKS 6
#define CHUNK_RES (TILE_RES / NUM_CHUNKS)
#define CHUNK_WORLD_SIZE (TILE_WORLD_SIZE / NUM_CHUNKS)

#define FREE_CAM_MOUSE_SENS 0.1
#define FREE_CAM_SPEED 100
#define FREE_CAM_FAST_MULTIPLIER 100

GLuint compile_shader(const char *path, GLenum type) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Missing shader %s\n", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *src = malloc(sz + 1);
    fread(src, 1, sz, f);
    fclose(f);
    src[sz] = 0;

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, (const char **)&src, NULL);
    glCompileShader(sh);

    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error in %s:\n%s\n", path, log);
        exit(1);
    }
    free(src);
    return sh;
}

GLuint load_program(const char *vs, const char *fs) {
    GLuint v = compile_shader(vs, GL_VERTEX_SHADER);
    GLuint f = compile_shader(fs, GL_FRAGMENT_SHADER);

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Link error:\n%s\n", log);
        exit(1);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

struct mesh gen_plane(float w, float h, int resx, int resz) {
    int vx = resx + 1;
    int vz = resz + 1;
    int vcount = vx * vz;

    float *verts = malloc(sizeof(float) * vcount * 5);
    unsigned *indices = malloc(sizeof(unsigned) * resx * resz * 6);

    int idx = 0;
    for (int z = 0; z < vz; z++)
        for (int x = 0; x < vx; x++) {
            float px = ((float)x / resx - 0.5f) * w;
            float pz = ((float)z / resz - 0.5f) * h;
            verts[idx++] = px;
            verts[idx++] = 0.0f;
            verts[idx++] = pz;
            verts[idx++] = (float)x / (vx - 1);
            verts[idx++] = (float)z / (vz - 1);
        }

    idx = 0;
    for (int z = 0; z < resz; z++)
        for (int x = 0; x < resx; x++) {
            int i = z * vx + x;
            indices[idx++] = i;
            indices[idx++] = i + vx;
            indices[idx++] = i + 1;
            indices[idx++] = i + 1;
            indices[idx++] = i + vx;
            indices[idx++] = i + vx + 1;
        }

    struct mesh m;
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vcount * 5 * sizeof(float), verts, GL_STATIC_DRAW);

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, resx * resz * 6 * sizeof(unsigned), indices,
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)(3 * sizeof(float)));

    m.index_count = resx * resz * 6;

    free(verts);
    free(indices);
    return m;
}

vec4s get_joystick_inputs() {
    float pitch = -(float)SDL_GetJoystickAxis(joy, 1) / SDL_JOYSTICK_AXIS_MIN;
    float roll = -(float)SDL_GetJoystickAxis(joy, 0) / SDL_JOYSTICK_AXIS_MIN;
    float yaw = (float)SDL_GetJoystickAxis(joy, 2) / SDL_JOYSTICK_AXIS_MIN;
    float throttle = (((float)SDL_GetJoystickAxis(joy, 3) / SDL_JOYSTICK_AXIS_MIN) + 1.0) / 2.0;
    return (vec4s){pitch, yaw, roll, throttle};
}

void update(float dt) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (freecam) {
        vec3s move = {0};
        if (keys[SDL_SCANCODE_W])
            move.z += 1;
        if (keys[SDL_SCANCODE_S])
            move.z -= 1;
        if (keys[SDL_SCANCODE_A])
            move.x -= 1;
        if (keys[SDL_SCANCODE_D])
            move.x += 1;
        if (keys[SDL_SCANCODE_SPACE])
            move.y += 1;
        if (keys[SDL_SCANCODE_LCTRL])
            move.y -= 1;

        vec3s world_move = glms_vec3_add(
            glms_vec3_scale(ac.forward, move.z),
            glms_vec3_add(glms_vec3_scale(ac.right, move.x), glms_vec3_scale(ac.up, move.y)));

        float speed = FREE_CAM_SPEED;
        if (keys[SDL_SCANCODE_LSHIFT])
            speed *= FREE_CAM_FAST_MULTIPLIER;

        float ds = speed * dt; // m/s → meters per frame
        world_move = glms_vec3_scale(world_move, ds);

        // Convert meters -> geodetic update
        ac.lat += world_move.z / 111320.0;                          // north (ENU y)
        ac.lon += world_move.x / (111320.0 * cos(glm_rad(ac.lat))); // east (ENU x)
        ac.height += world_move.y;                                  // up is ENU z

        // pitch
        float pitch_angle = -mouse_dy * FREE_CAM_MOUSE_SENS * dt;
        ac.forward = glms_vec3_rotate(ac.forward, pitch_angle, ac.right);
        ac.right = glms_vec3_normalize(glms_vec3_cross(ac.forward, ac.up));
        // yaw
        float yaw_angle = -mouse_dx * FREE_CAM_MOUSE_SENS * dt;
        ac.forward = glms_vec3_rotate(ac.forward, yaw_angle, ac.up);
    } else {
        aircraft_update(&ac, dt);

        vec4s joy_inputs = get_joystick_inputs();
        aircraft_pitch(&ac, glm_rad(joy_inputs.x));
        aircraft_yaw(&ac, glm_rad(joy_inputs.y));
        aircraft_roll(&ac, glm_rad(joy_inputs.z));
        ac.throttle = joy_inputs.w;
    }

    // update world offset based on aircraft
    double lat0 = 0.0;
    double lon0 = 0.0;
    vec3s originECEF = geodetic_to_ecef(lat0, lon0, 0.0);
    vec3s enu =
        geodetic_to_enu(glm_rad(ac.lat), glm_rad(ac.lon), ac.height, lat0, lon0, originECEF);

    world_offset.x = enu.x; // east
    world_offset.y = enu.z; // up
    world_offset.z = enu.y; // north
}

void render_ui() {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    // glOrtho(0, w, 0, h, -1, 1);
    // glMatrixMode(GL_MODELVIEW);
    // glLoadIdentity();

    // glBegin(GL_TRIANGLES);
    // glColor3f(1.0f, 0.0f, 0.0f);
    // glVertex2f(100, 100);
    // glVertex2f(200, 100);
    // glVertex2f(100, 200);
    // glEnd();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();
    {
        igSetNextWindowPos((ImVec2_c){10, 10}, ImGuiCond_Always, (ImVec2_c){0, 0});
        igSetNextWindowSize((ImVec2_c){300, 0}, ImGuiCond_Always);
        igSetNextWindowBgAlpha(0.35);
        igBegin("pos", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
        igText("LAT: %.5f°", ac.lat);
        igText("LON: %.5f°", ac.lon);
        igText("HGT: %.2fm", ac.height);
        igText("X:   %.2f", world_offset.x);
        igText("Y:   %.2f", world_offset.y);
        igText("Z:   %.2f", world_offset.z);
        igEnd();

        igSetNextWindowPos((ImVec2_c){w - 10, 10}, ImGuiCond_Always, (ImVec2_c){1, 0});
        igSetNextWindowSize((ImVec2_c){300, 0}, ImGuiCond_Always);
        igSetNextWindowBgAlpha(0.35);
        igBegin("inputs", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
        vec4s ji = get_joystick_inputs();
        igText("PITCH: %.2f", ji.x);
        igText("YAW: %.2f", ji.y);
        igText("ROLL: %.2f", ji.z);
        igText("THROTTLE: %.2f", ji.w);
        igEnd();
    }
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}

int16_t *load_dem(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    size_t count = TILE_RES * TILE_RES;
    int16_t *data = malloc(count * sizeof(*data));
    if (!data) {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, sizeof(*data), count, f);
    fclose(f);

    if (read != count) {
        fprintf(stderr, "File size mismatch: expected %zu values, got %zu\n", count, read);
        free(data);
        return NULL;
    }

    return data;
}

void make_terrain() {
    terrain_model.mesh = gen_plane(CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE, CHUNK_RES, CHUNK_RES);
    terrain_model.shader = load_program("shaders/terrain.vs", "shaders/terrain.fs");
    glUseProgram(terrain_model.shader);

    glUniform1f(glGetUniformLocation(terrain_model.shader, "heightmapSize"), TILE_RES);
    glUniform1f(glGetUniformLocation(terrain_model.shader, "chunkSize"), CHUNK_WORLD_SIZE);
    glUniform1i(glGetUniformLocation(terrain_model.shader, "chunkRes"), CHUNK_RES);
    glUniform3f(glGetUniformLocation(terrain_model.shader, "aircraftPos"), world_offset.x,
                world_offset.y, world_offset.z);
    glUniform3f(glGetUniformLocation(terrain_model.shader, "aircraftForward"), ac.forward.x,
                ac.forward.y, ac.forward.z);

    // Load DEM
    GLuint dem_tex;
    glGenTextures(1, &dem_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, dem_tex);
    int16_t *pixels = load_dem("terrain.raw");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16I, TILE_RES, TILE_RES, 0, GL_RED_INTEGER, GL_SHORT,
                 pixels);
    free(pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void render() {
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1, 0.2, 0.7, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4s view = glms_look(GLMS_VEC3_ZERO, ac.forward, ac.up);
    mat4s proj = glms_perspective(glm_rad(60.0f), (float)w / (float)h, 0.1f, 1000000.0f);

    glUseProgram(terrain_model.shader);
    int tileMinX = 0;
    int tileMinZ = 0;
    int tileMaxX = NUM_CHUNKS - 1;
    int tileMaxZ = NUM_CHUNKS - 1;
    int tx = world_offset.x / CHUNK_WORLD_SIZE;
    int tz = world_offset.z / CHUNK_WORLD_SIZE;

    int viewDist = 5;
    for (int dz = -viewDist; dz <= viewDist; dz++) {
        for (int dx = -viewDist; dx <= viewDist; dx++) {
            int cx = tx + dx;
            int cz = tz + dz;

            if (cx < tileMinX || cx > tileMaxX || cz < tileMinZ || cz > tileMaxZ)
                continue;

            glUniform2f(glGetUniformLocation(terrain_model.shader, "chunkOffset"), (float)cx,
                        (float)cz);

            vec3s world_pos = {cx * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f, 0,
                               cz * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f};

            world_pos = glms_vec3_sub(world_pos, world_offset);

            // draw
            mat4s model = glms_translate(GLMS_MAT4_IDENTITY, world_pos);
            mat4s mvp = glms_mat4_mulN((mat4s *[]){&proj, &view, &model}, 3);
            glUniformMatrix4fv(glGetUniformLocation(terrain_model.shader, "mvp"), 1, GL_FALSE,
                               (float *)mvp.raw);
            glBindVertexArray(terrain_model.mesh.vao);
            glDrawElements(GL_TRIANGLES, terrain_model.mesh.index_count, GL_UNSIGNED_INT, 0);
        }
    }

    render_ui();

    SDL_GL_SwapWindow(window);
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);
    uint32_t now = SDL_GetTicks();
    float dt = (now - last_tick) / 1000.0f;
    last_tick = now;

    update(dt);
    render();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e) {
    ImGui_ImplSDL3_ProcessEvent(e);

    switch (e->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.scancode == SDL_SCANCODE_C) {
            freecam = !freecam;
            ac.up = (vec3s){0, 1, 0};
        }
        return SDL_APP_CONTINUE;
    case SDL_EVENT_JOYSTICK_ADDED:
        joy = SDL_OpenJoystick(e->jdevice.which);
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
    last_tick = SDL_GetTicks();

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    window =
        SDL_CreateWindow("synvis", 1024, 768,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_SetWindowRelativeMouseMode(window, true);
    SDL_HideCursor();

    SDL_GL_SetSwapInterval(1);
    glctx = SDL_GL_CreateContext(window);
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return 1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // init imgui
    ImGuiContext *imguictx = igCreateContext(NULL);
    ImGuiIO *ioptr = igGetIO_ContextPtr(imguictx);
    ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGuiStyle *style = igGetStyle();
    style->FontScaleDpi = SDL_GetWindowDisplayScale(window);
    ImGui_ImplSDL3_InitForOpenGL(window, glctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    make_terrain();

    return 0;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_CloseJoystick(joy);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
