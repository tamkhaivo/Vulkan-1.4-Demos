#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float clusterID;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 clusterParams; // x: time, y: clusterCount, z: lodLevel
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(2.0, 3.0, 2.0) - fragWorldPos);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // Unique per-cluster color palette
    float cid = clusterID;
    vec3 clusterColor = 0.5 + 0.5 * cos(cid * 1.5 + vec3(0.0, 2.0, 4.0));

    // Dynamic Blinn-Phong lighting with cluster boundary highlights
    vec3 shaded = (vec3(0.15) + diff * 0.7 + spec * 0.3) * clusterColor;
    outColor = vec4(shaded, 1.0);
}
