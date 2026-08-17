#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant, scalar) uniform MasterPushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    float time;
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);

    // Multi-light dynamic forward shading
    vec3 light1Pos = vec3(2.5 * sin(pc.time * 1.1), 1.8, 2.5 * cos(pc.time * 1.1));
    vec3 light2Pos = vec3(2.5 * cos(pc.time * 0.9), -1.2, 2.5 * sin(pc.time * 0.9));
    vec3 light3Pos = vec3(0.0, 2.5 * sin(pc.time * 1.3), 2.5 * cos(pc.time * 1.3));

    vec3 L1 = normalize(light1Pos - fragWorldPos);
    vec3 L2 = normalize(light2Pos - fragWorldPos);
    vec3 L3 = normalize(light3Pos - fragWorldPos);

    vec3 H1 = normalize(L1 + V);
    vec3 H2 = normalize(L2 + V);
    vec3 H3 = normalize(L3 + V);

    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0);
    float diff3 = max(dot(N, L3), 0.0);

    float spec1 = pow(max(dot(N, H1), 0.0), 32.0);
    float spec2 = pow(max(dot(N, H2), 0.0), 32.0);
    float spec3 = pow(max(dot(N, H3), 0.0), 32.0);

    vec3 color1 = vec3(1.0, 0.5, 0.2) * (diff1 + spec1 * 0.7);
    vec3 color2 = vec3(0.2, 0.8, 1.0) * (diff2 + spec2 * 0.7);
    vec3 color3 = vec3(0.9, 0.3, 0.9) * (diff3 + spec3 * 0.7);

    vec3 ambient = vec3(0.06, 0.06, 0.09) * fragColor;
    vec3 finalColor = ambient + fragColor * (color1 + color2 + color3);

    // ACES-style Tone Mapping
    finalColor = finalColor / (finalColor + vec3(1.0));
    outColor = vec4(finalColor, 1.0);
}
