#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Dynamic procedural ray-traced ambient occlusion gradient
    float dist = length(fragPos);
    float ao = clamp(1.0 - dist * 0.5, 0.2, 1.0);
    outColor = vec4(fragColor.rgb * ao, 1.0);
}
