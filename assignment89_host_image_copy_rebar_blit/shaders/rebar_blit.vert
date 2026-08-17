#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 blitParams; // x: time
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float time;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    fragUV = inUV;
    time = pc.blitParams.x;
}
