#version 330

uniform vec3 lightDir;
uniform float heightmapSize;
uniform float chunkSize;

in float waterDist;
in vec3 normal;
in vec3 fragWorldPos;

out vec4 fragColor;

const vec3 WHITE = vec3(0.8, 0.8, 0.792);
const vec3 GRAY = vec3(0.592, 0.592, 0.592);
const vec3 BROWN3 = vec3(0.545, 0.235, 0.016);
const vec3 BROWN2 = vec3(0.553, 0.298, 0.086);
const vec3 BROWN1 = vec3(0.588, 0.388, 0.141);
const vec3 ORANGE = vec3(0.769, 0.58, 0.251);
const vec3 YELLOW = vec3(0.784, 0.694, 0.278);
const vec3 GREEN = vec3(0.255, 0.416, 0.055);
const vec3 CYAN = vec3(0.306, 0.6, 0.573);
const vec3 BLUE = vec3(0.145, 0.329, 0.408);
const vec3 DARKBLUE = vec3(0.008, 0.004, 0.278);

#define M2FT(m) ((m) / 0.3048f)

vec3 colorFromAltitude(float y) {
    float alt = M2FT(y);
    float levels[10] =
        float[](-2000.0, -500.0, 500.0, 2000.0, 3000.0, 6000.0, 8000.0, 10500.0, 27000.0, 100000.0);
    vec3 colors[10] =
        vec3[](BLUE, CYAN, GREEN, YELLOW, ORANGE, BROWN1, BROWN2, BROWN3, GRAY, WHITE);

    for (int i = 0; i < 9; i++) {
        if (alt < levels[i + 1]) {
            float t = (alt - levels[i]) / (levels[i + 1] - levels[i]);
            return mix(colors[i], colors[i + 1], clamp(t, 0.0, 1.0));
        }
    }

    return WHITE;
}

void main() {
    vec3 base = colorFromAltitude(fragWorldPos.y);
    float d = clamp(waterDist * heightmapSize, 0, 1);
    vec3 color = mix(DARKBLUE, base, d);

    // shading
    vec3 norm = normalize(normal);
    float diff = max(dot(norm, lightDir), 0.2);
    color *= diff;

    // grid
    // 1 arc-minute spacing
    float spacingNS = 1855.0;
    float spacingEW = 1855.0 * cos(radians(fragWorldPos.z / 111320.0));
    float lineWidth = 10.0;

    if (mod(fragWorldPos.z, spacingNS) < lineWidth || mod(fragWorldPos.x, spacingEW) < lineWidth) {
        color *= 0.5;
    }

    fragColor = vec4(color, 1.0);
}
