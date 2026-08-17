#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;

void main() {
    outAlbedo = vec4(fragColor, 1.0);
    outNormal = vec4(normalize(fragNormal), 1.0);
}
