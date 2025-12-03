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

static SDL_Window *window;
static SDL_GLContext glctx;
static SDL_Joystick *joy;
static Uint64 last_tick;
static float mouse_dx, mouse_dy;

#define TILE_RES 1200
#define TILE_WORLD_SIZE (111320 * cos(glm_rad(44)))
#define NUM_CHUNKS 6
#define CHUNK_RES (TILE_RES / NUM_CHUNKS)
#define CHUNK_WORLD_SIZE (TILE_WORLD_SIZE / NUM_CHUNKS)

#define CAMERA_SENS 0.1
#define JOYSTICK_DEADZONE 0.1

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

typedef struct {
    GLuint vao, vbo, ebo;
    GLsizei indexCount;
} Mesh;

Mesh gen_plane(float w, float h, int resx, int resz) {
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

    Mesh m;
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

    m.indexCount = resx * resz * 6;

    free(verts);
    free(indices);
    return m;
}

struct aircraft_state ac = {
    .speed = 200,
    .lat = 0,
    .lon = 0,
    .alt = 0,
    .forward = {0, 0, 1},
    .up = {0, 1, 0},
    .right = {-1, 0, 0},
};

vec3s world_offset = {0, 0, 0};

bool freecam = true;
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

        float speed = 100;
        if (keys[SDL_SCANCODE_LSHIFT])
            speed *= 100;

        float ds = speed * dt; // m/s → meters per frame
        world_move = glms_vec3_scale(world_move, ds);

        // Convert meters -> geodetic update
        ac.lat += world_move.z / 111320.0;                          // north (ENU y)
        ac.lon += world_move.x / (111320.0 * cos(glm_rad(ac.lat))); // east (ENU x)
        ac.alt += world_move.y;                                     // up is ENU z

        // pitch
        float pitch_angle = -mouse_dy * CAMERA_SENS * dt;
        ac.forward = glms_vec3_rotate(ac.forward, pitch_angle, ac.right);
        ac.right = glms_vec3_normalize(glms_vec3_cross(ac.forward, ac.up));
        // yaw
        float yaw_angle = -mouse_dx * CAMERA_SENS * dt;
        ac.forward = glms_vec3_rotate(ac.forward, yaw_angle, ac.up);
    } else {
        aircraft_update(&ac, dt);

        float pitch = (float)SDL_GetJoystickAxis(joy, 1) / SDL_JOYSTICK_AXIS_MIN;
        aircraft_pitch(&ac, glm_rad(-pitch));
        float roll = (float)SDL_GetJoystickAxis(joy, 0) / SDL_JOYSTICK_AXIS_MIN;
        aircraft_roll(&ac, glm_rad(-roll));
        float yaw = (float)SDL_GetJoystickAxis(joy, 2) / SDL_JOYSTICK_AXIS_MIN;
        aircraft_yaw(&ac, glm_rad(yaw));
    }

    // update world offset based on aircraft
    double lat0 = 0.0;
    double lon0 = 0.0;
    vec3s originECEF = geodetic_to_ecef(lat0, lon0, 0.0);
    vec3s enu = geodetic_to_enu(glm_rad(ac.lat), glm_rad(ac.lon), ac.alt, lat0, lon0, originECEF);

    world_offset.x = enu.x; // east
    world_offset.y = enu.z; // up
    world_offset.z = enu.y; // north
}

float *load_dem(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    size_t count = TILE_RES * TILE_RES;
    float *data = malloc(count * sizeof(float));
    if (!data) {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, sizeof(float), count, f);
    fclose(f);

    if (read != count) {
        fprintf(stderr, "File size mismatch: expected %zu floats, got %zu\n", count, read);
        free(data);
        return NULL;
    }

    return data;
}

void render_ui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();
    {
        igSetWindowPos_Vec2((ImVec2_c){10, 5}, 0);
        igBegin("pos", NULL, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
        igText("poop");
        igEnd();
    }
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}

void render() {
    static GLuint program = 0;
    static Mesh plane;
    if (!program) {
        program = load_program("shaders/terrain.vs", "shaders/terrain.fs");
        glUseProgram(program);

        glUniform1f(glGetUniformLocation(program, "heightmapSize"), TILE_RES);
        glUniform1f(glGetUniformLocation(program, "chunkSize"), CHUNK_WORLD_SIZE);
        glUniform1i(glGetUniformLocation(program, "chunkRes"), CHUNK_RES);
        glUniform3f(glGetUniformLocation(program, "aircraftPos"), world_offset.x, world_offset.y,
                    world_offset.z);
        glUniform3f(glGetUniformLocation(program, "aircraftForward"), ac.forward.x, ac.forward.y,
                    ac.forward.z);

        // Load DEM
        GLuint dem_tex;
        glGenTextures(1, &dem_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dem_tex);
        float *pixels = load_dem("dem.raw");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, TILE_RES, TILE_RES, 0, GL_RED, GL_FLOAT, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glUniform1i(glGetUniformLocation(program, "heightmap"), 0);
        free(pixels);

        plane = gen_plane(CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE, CHUNK_RES, CHUNK_RES);
    }

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.1, 0.2, 0.7, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4s view = glms_look(GLMS_VEC3_ZERO, ac.forward, ac.up);
    mat4s proj = glms_perspective(glm_rad(60.0f), (float)w / (float)h, 0.1f, 1000000.0f);

    glUseProgram(program);
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

            glUniform2f(glGetUniformLocation(program, "chunkOffset"), (float)cx, (float)cz);

            vec3s world_pos = {cx * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f, 0,
                               cz * CHUNK_WORLD_SIZE + CHUNK_WORLD_SIZE * 0.5f};

            world_pos = glms_vec3_sub(world_pos, world_offset);

            // draw
            mat4s model = glms_translate(GLMS_MAT4_IDENTITY, world_pos);
            mat4s mvp = glms_mat4_mulN((mat4s *[]){&proj, &view, &model}, 3);
            glUniformMatrix4fv(glGetUniformLocation(program, "mvp"), 1, GL_FALSE, (float *)mvp.raw);
            glBindVertexArray(plane.vao);
            glDrawElements(GL_TRIANGLES, plane.indexCount, GL_UNSIGNED_INT, 0);
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
        if (e->key.scancode == SDL_SCANCODE_C)
            freecam = !freecam;
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

    return 0;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_CloseJoystick(joy);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
