#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 simParams; // x: time, y: frequency, z: damping
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    // Dynamic physics wave deformation simulation
    vec3 p = inPos;
    float t = pc.simParams.x;
    float wave = sin(p.x * 8.0 + t * 4.0) * cos(p.z * 8.0 + t * 3.0) * 0.15;
    p.y += wave;

    vec4 worldPos = pc.model * vec4(p, 1.0);
    gl_Position = pc.mvp * vec4(p, 1.0);

    fragNormal = mat3(pc.model) * inNormal;
    fragColor = inColor + vec3(0.1, 0.3 * wave, 0.4 * abs(wave));
    fragWorldPos = worldPos.xyz;
}
