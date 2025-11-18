#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define SPEED 3.0f       // units per second
#define SENSITIVITY 0.1f // mouse sensitivity

#define TERRAIN_SIZE 50
#define TERRAIN_SCALE 1.0f

float heightmap[TERRAIN_SIZE][TERRAIN_SIZE];

// Vertex shader
const char *vertexShaderSrc = "#version 330 core\n"
                              "layout(location=0) in vec3 pos;\n"
                              "uniform mat4 mvp;\n"
                              "out float height;\n"
                              "void main() { gl_Position = mvp * vec4(pos,1.0); height = pos.y; }";

// Fragment shader
const char *fragmentShaderSrc = "#version 330 core\n"
                                "out vec4 FragColor;\n"
                                "in float height;\n"
                                "void main() { vec3 color = mix(vec3(0.0, 0.5, 0.0), vec3(0.5, "
                                "0.25, 0.0), (height + 1.0) * 0.5);"
                                "FragColor = vec4(color,1.0); }";

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window *win = SDL_CreateWindow("Cube FPS Camera", 800, 600, SDL_WINDOW_OPENGL);
    SDL_SetWindowRelativeMouseMode(win, true);

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    glewInit();

    // Compile shaders
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSrc, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSrc, NULL);
    glCompileShader(fs);
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
    vec3 camPos = {0.0f, 0.0f, 3.0f};
    float yaw = -90.0f; // facing -Z
    float pitch = 0.0f;
    vec3 camFront = {0.0f, 0.0f, -1.0f};
    vec3 camUp = {0.0f, 1.0f, 0.0f};

    bool quit = false;
    uint64_t lastTime = SDL_GetTicks();

    int width, height;
    SDL_GetWindowSize(win, &width, &height);
    glViewport(0, 0, width, height);

    while (!quit) {
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

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
        camFront[0] = cosf(glm_rad(yaw)) * cosf(glm_rad(pitch));
        camFront[1] = sinf(glm_rad(pitch));
        camFront[2] = sinf(glm_rad(yaw)) * cosf(glm_rad(pitch));
        glm_vec3_normalize(camFront);

        // Movement
        vec3 tmp;
        if (state[SDL_SCANCODE_W]) {
            glm_vec3_scale(camFront, SPEED * deltaTime, tmp);
            glm_vec3_add(camPos, tmp, camPos);
        }
        if (state[SDL_SCANCODE_S]) {
            glm_vec3_scale(camFront, SPEED * deltaTime, tmp);
            glm_vec3_sub(camPos, tmp, camPos);
        }
        if (state[SDL_SCANCODE_A]) {
            glm_vec3_cross(camFront, camUp, tmp);
            glm_vec3_normalize(tmp);
            glm_vec3_scale(tmp, SPEED * deltaTime, tmp);
            glm_vec3_sub(camPos, tmp, camPos);
        }
        if (state[SDL_SCANCODE_D]) {
            glm_vec3_cross(camFront, camUp, tmp);
            glm_vec3_normalize(tmp);
            glm_vec3_scale(tmp, SPEED * deltaTime, tmp);
            glm_vec3_add(camPos, tmp, camPos);
        }

        // Matrices
        mat4 model, view, proj, mvp;
        glm_mat4_identity(model);
        glm_perspective(glm_rad(45.0f), (float)width / height, 0.1f, 100.0f, proj);
        vec3 target;
        glm_vec3_add(camPos, camFront, target);
        glm_lookat(camPos, target, camUp, view);
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
