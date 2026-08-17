#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant, scalar) uniform MegaPushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    float time;
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);

    // Multi-light dynamic forward shading
    vec3 light1Pos = vec3(2.0 * sin(pc.time), 1.5, 2.0 * cos(pc.time));
    vec3 light2Pos = vec3(2.0 * cos(pc.time * 0.8), -1.0, 2.0 * sin(pc.time * 0.8));

    vec3 L1 = normalize(light1Pos - fragWorldPos);
    vec3 L2 = normalize(light2Pos - fragWorldPos);
    vec3 H1 = normalize(L1 + V);
    vec3 H2 = normalize(L2 + V);

    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0);
    float spec1 = pow(max(dot(N, H1), 0.0), 32.0);
    float spec2 = pow(max(dot(N, H2), 0.0), 32.0);

    vec3 color1 = vec3(1.0, 0.6, 0.2) * (diff1 + spec1 * 0.6);
    vec3 color2 = vec3(0.2, 0.7, 1.0) * (diff2 + spec2 * 0.6);
    vec3 ambient = vec3(0.05, 0.05, 0.08) * fragColor;

    vec3 finalColor = ambient + fragColor * (color1 + color2);

    // Tone mapping
    finalColor = finalColor / (finalColor + vec3(1.0));
    outColor = vec4(finalColor, 1.0);
}
