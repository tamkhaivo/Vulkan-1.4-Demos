#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Exact geometric normal visualization extracted from position fetch
    vec3 N = normalize(fragNormal);
    vec3 geomColor = N * 0.5 + 0.5;
    outColor = vec4(geomColor, 1.0);
}
