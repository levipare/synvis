#version 330

uniform mat4 mvp;

uniform vec3 aircraftPos;
uniform vec3 aircraftForward;
uniform sampler2D heightmap;
uniform sampler2D watermask;
uniform float heightmapSize;
uniform vec2 chunkOffset;
uniform float chunkRes;
uniform float chunkSize;

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out float waterDist;
out vec3 normal;
out vec3 fragWorldPos;

void main() {
    vec2 uv = ((vertexTexCoord + chunkOffset) * chunkRes + 0.5) / heightmapSize;
    float h = texture(heightmap, uv).r;
    vec3 displacedPosition = vertexPosition + vec3(0.0, h, 0.0);

    // compute normal
    float texelSize = 1.0 / heightmapSize;
    float hL = texture(heightmap, uv + vec2(-texelSize, 0.0)).r;
    float hR = texture(heightmap, uv + vec2(texelSize, 0.0)).r;
    float hD = texture(heightmap, uv + vec2(0.0, -texelSize)).r;
    float hU = texture(heightmap, uv + vec2(0.0, texelSize)).r;
    // Convert slope to meters per sample
    float spacingX = 90; // meters per sample longitude
    float spacingZ = 90; // meters per sample latitude
    float dx = (hL - hR) / spacingX;
    float dz = (hD - hU) / spacingZ;
    normal = normalize(vec3(dx, 2.0, dz));

    vec3 chunkOrigin = vec3(chunkOffset.x * chunkSize, 0.0, chunkOffset.y * chunkSize);
    fragWorldPos = chunkOrigin + displacedPosition;
    waterDist = texture(watermask, uv).r;

    gl_Position = mvp * vec4(displacedPosition, 1.0);
}
