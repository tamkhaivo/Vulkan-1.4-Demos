#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.4, 0.8, 0.6));
    vec3 V = normalize(vec3(0.0, 0.0, 1.0));
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.25);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 diffuse = fragColor * NdotL;
    vec3 specular = vec3(1.0) * spec * 0.5;

    outColor = vec4(diffuse + specular, 1.0);
}
