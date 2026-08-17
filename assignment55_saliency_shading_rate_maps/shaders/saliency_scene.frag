#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Dynamic procedural pattern highlighting foveated saliency detail
    float pattern = sin(fragPos.x * 20.0) * cos(fragPos.y * 20.0);
    vec3 col = fragColor.rgb + vec3(pattern * 0.15);
    outColor = vec4(col, 1.0);
}
