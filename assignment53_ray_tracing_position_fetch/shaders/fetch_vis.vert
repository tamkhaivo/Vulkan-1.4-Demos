#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 displayMode; // x: 0 = geom normal, 1 = barycentrics
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    fragNormal = inNormal;
    fragPos = inPos;
}
