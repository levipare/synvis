#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <cglm/struct/vec3.h>
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
#include "mesh.h"

struct tile {
    int16_t lat, lon, xres, yres;
    int16_t *data;
    GLuint tex;
};

struct {
    struct mesh mesh;
    GLuint shader;
    struct tile *tiles;
    size_t num_tiles;
} terrain_model;

struct {
    struct mesh mesh;
    GLuint shader;
} ellipsoid_model;

static SDL_Window *window;
static SDL_GLContext glctx;
static SDL_Joystick *joy;
static Uint64 last_tick;
static float mouse_dx, mouse_dy;
static ImGuiIO *igIO;

static bool draw_ellipsoid = true;
static bool draw_ui = false;
static bool freecam = true;

static struct aircraft_state ac = {
    .max_speed = 200,
    .throttle = 0,
    .lat = 44,
    .lon = -74,
    .pos = {1266684.95 / 1000, -4417455.4 / 1000, 4408091.61 / 1000},
    .forward = {1, 0, 0},
    .up = {0, 0, 1},
    .right = {0, 0, 1},
};

#define FREE_CAM_MOUSE_SENS 0.1
#define FREE_CAM_SPEED 1
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

vec4s get_joystick_inputs() {
    float pitch = (float)-SDL_GetJoystickAxis(joy, 1) / SDL_JOYSTICK_AXIS_MIN;
    float roll = (float)-SDL_GetJoystickAxis(joy, 0) / SDL_JOYSTICK_AXIS_MIN;
    float yaw = (float)SDL_GetJoystickAxis(joy, 2) / SDL_JOYSTICK_AXIS_MIN;
    float throttle = (((float)SDL_GetJoystickAxis(joy, 3) / SDL_JOYSTICK_AXIS_MIN) + 1.0) / 2.0;
    return (vec4s){pitch, yaw, roll, throttle};
}

void update(float dt) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (!igIO->WantCaptureMouse && SDL_GetWindowRelativeMouseMode(window)) {
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

            // pitch
            float pitch_angle = -mouse_dy * FREE_CAM_MOUSE_SENS * dt;
            ac.forward = glms_vec3_rotate(ac.forward, pitch_angle, ac.right);
            ac.right = glms_vec3_normalize(glms_vec3_cross(ac.forward, ac.up));
            // yaw
            float yaw_angle = -mouse_dx * FREE_CAM_MOUSE_SENS * dt;
            ac.forward = glms_vec3_rotate(ac.forward, yaw_angle, ac.up);

            vec3s world_move = glms_vec3_add(
                glms_vec3_scale(ac.forward, move.z),
                glms_vec3_add(glms_vec3_scale(ac.right, move.x), glms_vec3_scale(ac.up, move.y)));

            float speed = FREE_CAM_SPEED;
            if (keys[SDL_SCANCODE_LSHIFT])
                speed *= FREE_CAM_FAST_MULTIPLIER;
            float ds = speed * dt;
            world_move = glms_vec3_scale(world_move, ds);

            ac.pos = glms_vec3_add(ac.pos, world_move);
        } else {
            aircraft_update(&ac, dt);

            vec4s joy_inputs = get_joystick_inputs();
            aircraft_pitch(&ac, 0.5 * glm_rad(joy_inputs.x));
            aircraft_yaw(&ac, 0.5 * glm_rad(joy_inputs.y));
            aircraft_roll(&ac, 0.5 * glm_rad(joy_inputs.z));
            ac.throttle = joy_inputs.w;
        }
    }
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
        igText("X:   %.2f", ac.pos.x);
        igText("Y:   %.2f", ac.pos.y);
        igText("Z:   %.2f", ac.pos.z);
        igEnd();

        igBegin("Settings", NULL, 0);
        igCheckbox("Draw Ellipsoid", &draw_ellipsoid);
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

void load_tdb(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", path);
        return;
    }

    terrain_model.num_tiles = 0;
    while (1) {
        terrain_model.tiles =
            realloc(terrain_model.tiles, sizeof(struct tile) * (terrain_model.num_tiles + 1));
        struct tile *t = &terrain_model.tiles[terrain_model.num_tiles];
        if (fread(&t->lat, sizeof(int16_t), 1, fp) != 1)
            break;
        if (fread(&t->lon, sizeof(int16_t), 1, fp) != 1)
            break;
        if (fread(&t->xres, sizeof(int16_t), 1, fp) != 1)
            break;
        if (fread(&t->yres, sizeof(int16_t), 1, fp) != 1)
            break;
        t->data = malloc(sizeof(int16_t) * t->xres * t->yres);
        if (fread(t->data, sizeof(int16_t), t->xres * t->yres, fp) != t->xres * t->yres)
            break;

        terrain_model.num_tiles++;
    }
    printf("Number of tiles: %zu\n", terrain_model.num_tiles);
}

void render_ellipsoid(mat4s view, mat4s proj) {
    mat4s model = GLMS_MAT4_IDENTITY;
    mat4s mvp = glms_mat4_mulN((mat4s *[]){&proj, &view, &model}, 3);

    // draw
    glUseProgram(ellipsoid_model.shader);
    glUniformMatrix4fv(glGetUniformLocation(ellipsoid_model.shader, "mvp"), 1, GL_FALSE,
                       (float *)mvp.raw);
    glBindVertexArray(ellipsoid_model.mesh.vao);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES, ellipsoid_model.mesh.index_count, GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

#define TILE_RES 1200
void make_terrain() {
    // TODO account for tiles of smaller dimensions
    int vcount = TILE_RES * TILE_RES;
    float *verts = malloc(sizeof(float) * vcount);
    int quads = (TILE_RES - 1) * (TILE_RES - 1);
    int icount = quads * 6;
    unsigned int *indices = malloc(sizeof(unsigned int) * icount);
    int idx = 0;
    for (int y = 0; y < TILE_RES - 1; y++) {
        for (int x = 0; x < TILE_RES - 1; x++) {
            int i = y * TILE_RES + x;
            indices[idx++] = i;
            indices[idx++] = i + TILE_RES;
            indices[idx++] = i + 1;
            indices[idx++] = i + 1;
            indices[idx++] = i + TILE_RES;
            indices[idx++] = i + TILE_RES + 1;
        }
    }
    terrain_model.mesh = (struct mesh){
        .index_count = icount,
    };
    glGenVertexArrays(1, &terrain_model.mesh.vao);
    glBindVertexArray(terrain_model.mesh.vao);

    glGenBuffers(1, &terrain_model.mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, terrain_model.mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vcount * sizeof(float), verts, GL_STATIC_DRAW);

    glGenBuffers(1, &terrain_model.mesh.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain_model.mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, icount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    free(verts);
    free(indices);

    terrain_model.shader = load_program("shaders/terrain.vs", "shaders/terrain.fs");
    glUseProgram(terrain_model.shader);
    glUniform1f(glGetUniformLocation(terrain_model.shader, "A"), WGS84_A);
    glUniform1f(glGetUniformLocation(terrain_model.shader, "B"), WGS84_B);
    glUniform1f(glGetUniformLocation(terrain_model.shader, "E2"), WGS84_E2);

    // Load heightmaps
    load_tdb("terrain.tdb");
    for (int i = 0; i < terrain_model.num_tiles; i++) {
        struct tile *t = &terrain_model.tiles[i];
        glGenTextures(1, &t->tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t->tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16I, t->xres, t->yres, 0, GL_RED_INTEGER, GL_SHORT,
                     t->data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void render_terrain(mat4s view, mat4s proj) {
    mat4s model = GLMS_MAT4_IDENTITY;
    mat4s mvp = glms_mat4_mulN((mat4s *[]){&proj, &view, &model}, 3);

    // draw
    glUseProgram(terrain_model.shader);
    glUniformMatrix4fv(glGetUniformLocation(terrain_model.shader, "mvp"), 1, GL_FALSE,
                       (float *)mvp.raw);
    for (int i = 0; i < terrain_model.num_tiles; i++) {
        struct tile *t = &terrain_model.tiles[i];
        // TODO better distance determination
        vec3s ecef = geodetic_to_ecef(glm_rad(t->lat - 0.5), glm_rad(t->lon + 0.5), 0);
        float dist = glms_vec3_distance(ecef, ac.pos);
        if (dist < 200) {
            glUniform1f(glGetUniformLocation(terrain_model.shader, "lat"), t->lat);
            glUniform1f(glGetUniformLocation(terrain_model.shader, "lon"), t->lon);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, t->tex);
            glBindVertexArray(terrain_model.mesh.vao);
            glDrawElements(GL_TRIANGLES, terrain_model.mesh.index_count, GL_UNSIGNED_INT, 0);
        }
    }
}

void render() {
    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1, 0.2, 0.7, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4s view = glms_look(GLMS_VEC3_ZERO, ac.forward, ac.up);
    view = glms_translate(view, glms_vec3_negate(ac.pos));
    mat4s proj = glms_perspective(glm_rad(60.0f), (float)w / (float)h, 0.1f, 1000);

    render_terrain(view, proj);
    if (draw_ellipsoid)
        render_ellipsoid(view, proj);
    if (draw_ui)
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
        }
        if (e->key.scancode == SDL_SCANCODE_ESCAPE) {
            if (SDL_GetWindowRelativeMouseMode(window)) {
                SDL_SetWindowRelativeMouseMode(window, false);
            } else {
                SDL_SetWindowRelativeMouseMode(window, true);
            }
        }
        if (e->key.scancode == SDL_SCANCODE_TAB) {
            draw_ui = !draw_ui;
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
    // glEnable(GL_CULL_FACE);

    // init imgui
    ImGuiContext *imguictx = igCreateContext(NULL);
    igIO = igGetIO_ContextPtr(imguictx);
    igIO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGuiStyle *style = igGetStyle();
    style->FontScaleDpi = SDL_GetWindowDisplayScale(window);
    ImGui_ImplSDL3_InitForOpenGL(window, glctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    make_terrain();
    ellipsoid_model.mesh = gen_ellipsoid(WGS84_A, WGS84_B);
    ellipsoid_model.shader = load_program("shaders/ellipsoid.vs", "shaders/ellipsoid.fs");

    return 0;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    // TODO: cleanup tiles
    SDL_CloseJoystick(joy);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
