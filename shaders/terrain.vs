#version 330

uniform mat4 mvp;

uniform vec3 aircraftPos;
uniform vec3 aircraftForward;
uniform isampler2D heightmap;
uniform float heightmapSize;
uniform vec2 chunkOffset;
uniform int chunkRes;
uniform float chunkSize;

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out float water;
out vec3 normal;
out vec3 fragWorldPos;

void main() {
    vec2 uv = ((vertexTexCoord + chunkOffset) * chunkRes + 0.5) / heightmapSize;
    int texel = texture(heightmap, uv).r;
    water = float(texel & 1);
    float h = float(texel >> 1);
    vec3 displacedPosition = vertexPosition + vec3(0.0, h, 0.0);

    vec3 chunkOrigin = vec3(chunkOffset.x * chunkSize, 0.0, chunkOffset.y * chunkSize);
    fragWorldPos = chunkOrigin + displacedPosition;

    // normal
    float texelSize = 1.0 / heightmapSize;
    float hL = texture(heightmap, uv + vec2(-texelSize, 0.0)).r;
    float hR = texture(heightmap, uv + vec2(texelSize, 0.0)).r;
    float hD = texture(heightmap, uv + vec2(0.0, -texelSize)).r;
    float hU = texture(heightmap, uv + vec2(0.0, texelSize)).r;
    float spacingX = 90.0;
    float spacingZ = 90.0;
    float dx = (hL - hR) / spacingX;
    float dz = (hD - hU) / spacingZ;
    normal = normalize(vec3(dx, 2.0, dz));

    gl_Position = mvp * vec4(displacedPosition, 1.0);
}
