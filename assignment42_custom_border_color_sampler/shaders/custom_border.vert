#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float uvScale;
} pc;

layout(location = 0) out vec2 fragUV;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    // Expand UVs beyond [0, 1] range to trigger custom border clamping!
    fragUV = (inUV - 0.5) * pc.uvScale + 0.5;
}
