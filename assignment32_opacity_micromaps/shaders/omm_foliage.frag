#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Opacity Micromap procedural cutout simulation (leaf cutout pattern)
    vec2 p = fragPos * 4.0;
    float dist = length(fract(p) - vec2(0.5));
    if (dist > 0.42) {
        discard;
    }
    outColor = vec4(fragColor, 1.0);
}
