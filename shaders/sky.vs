#version 330

in vec2 vertexPosition;
out vec2 uv;

void main() {
    uv = (vertexPosition + 1.0) * 0.5; // convert clip-space xy to 0–1 UV
    gl_Position = vec4(vertexPosition, 0.0, 1.0);
}
