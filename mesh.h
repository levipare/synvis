#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include <cglm/struct.h>
#include <cglm/util.h>
#include <malloc.h>

struct mesh {
    GLuint vao, vbo, ebo;
    GLsizei index_count;
};

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

    struct mesh m = {
        .index_count = resx * resz * 6,
    };
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

    free(verts);
    free(indices);
    return m;
}

struct mesh gen_ellipsoid(double a, double b) {
    const double e2 = 1.0 - (b * b) / (a * a);

    int stacks = 180; // 1-degree latitude steps
    int slices = 360; // 1-degree longitude steps
    int vcount = (stacks + 1) * (slices + 1);
    float *verts = malloc(sizeof(float) * 3 * vcount);

    int i = 0;
    for (int lat = -90; lat <= 90; lat++) {
        float phi = glm_rad(lat);
        float cos_phi = cosf(phi);
        float sin_phi = sinf(phi);

        // radius of curvature in the prime vertical
        double N = a / sqrt(1.0 - e2 * sin_phi * sin_phi);

        for (int lon = 0; lon <= 360; lon++) {
            double theta = glm_rad(lon);
            double sin_theta = sin(theta);
            double cos_theta = cos(theta);

            double x = N * cos_phi * cos_theta;
            double y = N * cos_phi * sin_theta;
            double z = N * (1.0 - e2) * sin_phi;

            verts[i++] = (float)x;
            verts[i++] = (float)y;
            verts[i++] = (float)z;
        }
    }

    int icount = stacks * slices * 6;
    unsigned int *indices = malloc(sizeof(unsigned int) * icount);
    int idx = 0;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int row1 = i * (slices + 1);
            int row2 = (i + 1) * (slices + 1);

            int a = row1 + j;
            int b = row1 + j + 1;
            int c = row2 + j;
            int d = row2 + j + 1;

            // triangle 1
            indices[idx++] = a;
            indices[idx++] = c;
            indices[idx++] = b;

            // triangle 2
            indices[idx++] = b;
            indices[idx++] = c;
            indices[idx++] = d;
        }
    }

    struct mesh m = {
        .index_count = icount,
    };
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vcount * 3 * sizeof(float), verts, GL_STATIC_DRAW);

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, icount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

    free(verts);
    free(indices);

    return m;
}

#endif
