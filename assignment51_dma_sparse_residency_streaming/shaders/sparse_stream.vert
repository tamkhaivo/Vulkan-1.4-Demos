#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 streamParam; // x: residencyFraction, yzw: color tint
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    fragColor = vec4(inColor * pc.streamParam.yzw, pc.streamParam.x);
}
