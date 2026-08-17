#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Clustered subgroup reduction lattice pattern visualization
    float pattern = sin(fragPos.x * 30.0) * sin(fragPos.y * 30.0) * sin(fragPos.z * 30.0);
    vec3 col = fragColor.rgb + vec3(pattern * 0.12);
    outColor = vec4(col, 1.0);
}
