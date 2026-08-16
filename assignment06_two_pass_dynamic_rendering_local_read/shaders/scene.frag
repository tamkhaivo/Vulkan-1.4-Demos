#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(1.2, 2.0, 1.5));
    vec3 V = normalize(vec3(0.0, 0.0, 3.5) - fragWorldPos);
    vec3 H = normalize(L + V);

    float ambient = 0.22;
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.6;

    vec3 litColor = (ambient + diff) * fragColor + vec3(spec);
    outColor = vec4(litColor, 1.0);
}
