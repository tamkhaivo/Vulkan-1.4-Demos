#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(set = 0, binding = 0) uniform UniformData {
    mat4 mvp;
    vec4 tintColor;
} ubo;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    fragColor = ubo.tintColor * vec4(inColor, 1.0);
}
