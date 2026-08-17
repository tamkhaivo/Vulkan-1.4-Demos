#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant, scalar) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    float time;
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(2.0, 2.5, 2.0) - fragWorldPos);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // Telemetry memory safety pulse (valid BDA range indicator)
    float safePulse = 0.8 + 0.2 * sin(pc.time * 4.0);
    vec3 telemetryColor = fragColor * vec3(0.3, 0.9, 0.4) * safePulse;

    vec3 shaded = (vec3(0.1) + diff * 0.7 + spec * 0.4) * telemetryColor;
    outColor = vec4(shaded, 1.0);
}
