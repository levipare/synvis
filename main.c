#include <math.h>
#include <stdbool.h>

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <cglm/cglm.h>

#define SPEED 20.0f      // units per second
#define SENSITIVITY 0.1f // mouse sensitivity

#define TERRAIN_SIZE 400
#define TERRAIN_SCALE 1.0f

float heightmap[TERRAIN_SIZE][TERRAIN_SIZE];

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window *win = SDL_CreateWindow("Cube FPS Camera", 800, 600, SDL_WINDOW_OPENGL);
    SDL_SetWindowRelativeMouseMode(win, true);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    glewInit();

    // Compile shaders
    SDL_IOStream *io = SDL_IOFromFile("shaders/terrain.vert", "r");
    Sint64 vert_size = SDL_GetIOSize(io);
    GLchar *vert_src = SDL_malloc(sizeof(*vert_src) * (vert_size + 1));
    SDL_ReadIO(io, vert_src, vert_size);
    vert_src[vert_size] = '\0';
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, (const GLchar *const *)&vert_src, NULL);
    glCompileShader(vs);
    SDL_free(vert_src);

    io = SDL_IOFromFile("shaders/terrain.frag", "r");
    Sint64 frag_size = SDL_GetIOSize(io);
    GLchar *frag_src = SDL_malloc(sizeof(*frag_src) * (frag_size + 1));
    SDL_ReadIO(io, frag_src, frag_size);
    frag_src[frag_size] = '\0';
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, (const GLchar *const *)&frag_src, NULL);
    glCompileShader(fs);
    SDL_free(frag_src);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glUseProgram(program);
    GLint mvpLoc = glGetUniformLocation(program, "mvp");

    // fake terrain
    for (int z = 0; z < TERRAIN_SIZE; z++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            heightmap[z][x] = sinf(x * 0.2f) * cosf(z * 0.2f);
        }
    }

    GLfloat vertices[TERRAIN_SIZE * TERRAIN_SIZE * 3];
    GLuint indices[(TERRAIN_SIZE - 1) * (TERRAIN_SIZE - 1) * 6];

    int v = 0;
    for (int z = 0; z < TERRAIN_SIZE; z++) {
        for (int x = 0; x < TERRAIN_SIZE; x++) {
            vertices[v++] = x * TERRAIN_SCALE;
            vertices[v++] = heightmap[z][x];
            vertices[v++] = z * TERRAIN_SCALE;
        }
    }

    int idx = 0;
    for (int z = 0; z < TERRAIN_SIZE - 1; z++) {
        for (int x = 0; x < TERRAIN_SIZE - 1; x++) {
            int topLeft = z * TERRAIN_SIZE + x;
            int topRight = topLeft + 1;
            int bottomLeft = topLeft + TERRAIN_SIZE;
            int bottomRight = bottomLeft + 1;

            indices[idx++] = topLeft;
            indices[idx++] = bottomLeft;
            indices[idx++] = topRight;

            indices[idx++] = topRight;
            indices[idx++] = bottomLeft;
            indices[idx++] = bottomRight;
        }
    }

    GLuint VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glEnable(GL_DEPTH_TEST);

    // Camera state
    vec3 cam_pos = {0.0f, 5.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    vec3 cam_front = {0.0f, 0.0f, 0.0f};
    vec3 cam_up = {0.0f, 1.0f, 0.0f};

    bool quit = false;
    uint64_t lastTime = SDL_GetTicks();

    int width, height;
    SDL_GetWindowSize(win, &width, &height);
    glViewport(0, 0, width, height);

    while (!quit) {
        uint64_t current_time = SDL_GetTicks();
        float delta_time = (current_time - lastTime) / 1000.0f;
        lastTime = current_time;

        const bool *state = SDL_GetKeyboardState(NULL);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                quit = true;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
                quit = true;
            if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                width = e.window.data1;
                height = e.window.data2;
                glViewport(0, 0, width, height);
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                float xrel = e.motion.xrel;
                float yrel = e.motion.yrel;
                yaw += xrel * SENSITIVITY;
                pitch -= yrel * SENSITIVITY;
                if (pitch > 89.0f)
                    pitch = 89.0f;
                if (pitch < -89.0f)
                    pitch = -89.0f;
            }
        }

        // Update camera front vector
        cam_front[0] = cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
        cam_front[1] = sinf(glm_rad(pitch));
        cam_front[2] = sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));
        glm_vec3_normalize(cam_front);

        // Movement
        vec3 tmp;
        if (state[SDL_SCANCODE_W]) {
            glm_vec3_scale(cam_front, SPEED * delta_time, tmp);
            glm_vec3_add(cam_pos, tmp, cam_pos);
        }
        if (state[SDL_SCANCODE_S]) {
            glm_vec3_scale(cam_front, SPEED * delta_time, tmp);
            glm_vec3_sub(cam_pos, tmp, cam_pos);
        }
        if (state[SDL_SCANCODE_A]) {
            glm_vec3_cross(cam_front, cam_up, tmp);
            glm_vec3_normalize(tmp);
            glm_vec3_scale(tmp, SPEED * delta_time, tmp);
            glm_vec3_sub(cam_pos, tmp, cam_pos);
        }
        if (state[SDL_SCANCODE_D]) {
            glm_vec3_cross(cam_front, cam_up, tmp);
            glm_vec3_normalize(tmp);
            glm_vec3_scale(tmp, SPEED * delta_time, tmp);
            glm_vec3_add(cam_pos, tmp, cam_pos);
        }

        // Matrices
        mat4 model, view, proj, mvp;
        glm_mat4_identity(model);
        glm_perspective(glm_rad(45.0f), (float)width / height, 0.1f, 1000.0f, proj);
        vec3 target;
        glm_vec3_add(cam_pos, cam_front, target);
        glm_lookat(cam_pos, target, cam_up, view);
        glm_mat4_mulN((mat4 *[]){&proj, &view, &model}, 3, mvp);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, (const GLfloat *)mvp);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (TERRAIN_SIZE - 1) * (TERRAIN_SIZE - 1) * 6, GL_UNSIGNED_INT,
                       0);

        SDL_GL_SwapWindow(win);
    }

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
