#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 streamParams; // x: time, y: residentPageCount
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    fragNormal = mat3(pc.model) * inNormal;
    fragUV = inUV;
    fragWorldPos = (pc.model * vec4(inPos, 1.0)).xyz;
}
