#version 330 core

out vec4 fragColor;
in float height;

void main() {
    vec3 color = mix(vec3(0.0, 0.5, 0.0), vec3(0.5, 0.25, 0.0), (height + 1.0) * 0.5);
    fragColor = vec4(color, 1.0);
}
