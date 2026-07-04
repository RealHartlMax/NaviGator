#version 460 core

layout(location = 0) in vec3 inPosition;

uniform mat4 uView;
uniform mat4 uProj;

out VS_OUT {
    vec3 worldPos;
} vs_out;

void main() {
    vs_out.worldPos = inPosition;
    gl_Position = uProj * uView * vec4(inPosition, 1.0);
}
