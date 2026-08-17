#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.5, 0.8, 0.5));
    float NdotL = max(dot(N, L), 0.2);
    outColor = vec4(fragColor * NdotL, 1.0);
}
