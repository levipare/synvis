#version 330

in vec2 uv;
out vec4 fragColor;

uniform mat4 invView;
uniform mat4 invProj;

void main() {
    vec3 zenithColor = vec3(0.05f, 0.25f, 0.70f);
    vec3 horizonColor = vec3(0.60f, 0.70f, 0.90f);
    vec3 nadirColor = vec3(0.60f, 0.70f, 0.90f);
    float sharpness = 10.0;

    vec2 ndc = uv * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);

    vec4 viewDir = invProj * clip;
    viewDir /= viewDir.w;

    vec3 dir = normalize((invView * vec4(viewDir.xyz, 0.0)).xyz);
    float y = dir.y;

    float horizon = exp(-abs(y) * sharpness);

    float t = y * 0.5 + 0.5;
    vec3 tb = mix(nadirColor, zenithColor, t);
    vec3 col = mix(tb, horizonColor, horizon);
    fragColor = vec4(col, 1.0);
}
