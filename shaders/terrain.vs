#version 330

const float KM_SCALAR = 0.001;

uniform mat4 mvp;

uniform float A;
uniform float B;
uniform float E2;
uniform float lat;
uniform float lon;
uniform isampler2D heightmap;

out float water;
out float height;
out vec3 normal;

// lon degrees
// lat degrees
// h km
vec3 geodetic_to_ecef(float lat, float lon, float h) {
    float theta = radians(lon); // lon
    float phi = radians(lat);   // lat
    float sinphi = sin(phi);
    float N = A / sqrt(1.0 - E2 * sinphi * sinphi);

    vec3 earth_pos = vec3((N + h) * cos(phi) * cos(theta), (N + h) * cos(phi) * sin(theta),
                          (N * (1.0 - E2) + h) * sin(phi));

    return earth_pos;
}

void main() {
    ivec2 size = textureSize(heightmap, 0);
    int position_i = gl_VertexID / size.x;
    int position_j = gl_VertexID % size.x;
    ivec2 pixel = ivec2(position_j, position_i);
    int texel = texelFetch(heightmap, pixel, 0).r;
    water = float(texel & 1);
    height = float(texel >> 1);

    float plat = lat - float(position_i) / float(size.x - 1);
    float plon = lon + float(position_j) / float(size.y - 1);
    vec3 earth_pos = geodetic_to_ecef(plat, plon, height * KM_SCALAR);
    gl_Position = mvp * vec4(earth_pos, 1.0);

    // normal
    const ivec3 off = ivec3(-1, 0, 1);
    int left = texelFetchOffset(heightmap, pixel, 0, off.xy).r >> 1;
    int right = texelFetchOffset(heightmap, pixel, 0, off.zy).r >> 1;
    int down = texelFetchOffset(heightmap, pixel, 0, off.yx).r >> 1;
    int up = texelFetchOffset(heightmap, pixel, 0, off.yz).r >> 1;
    float dx_spacing = cos(radians(plat)) * radians(1.0 / size.x) * A * 1000;
    float dz_spacing = radians(1.0 / size.y) * A * 1000;
    float dx = (float(left) - float(right)) / dx_spacing;
    float dz = (float(down) - float(up)) / dz_spacing;
    normal = normalize(vec3(dx, 2.0, dz));
}
