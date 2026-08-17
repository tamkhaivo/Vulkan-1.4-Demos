#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.8));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.2);

    vec3 baseCol = vec3(0.2, 0.8, 0.9) + 0.3 * sin(fragPos * 6.0);
    outColor = vec4(baseCol * diff, 1.0);
}
