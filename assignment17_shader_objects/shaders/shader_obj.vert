#version 460

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform PushConstants {
    vec2 offset;
    float scale;
    float angle;
} push;

void main() {
    mat2 rot = mat2(cos(push.angle), -sin(push.angle), sin(push.angle), cos(push.angle));
    vec2 p = rot * (inPos * push.scale) + push.offset;
    gl_Position = vec4(p, 0.0, 1.0);
    fragColor = inColor;
}
