#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(buffer_reference, scalar) readonly buffer SceneUniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 viewPos;
};

struct BdaVertex {
    vec4 pos;
    vec4 color;
    vec4 normal;
};

layout(buffer_reference, scalar) readonly buffer VertexStream {
    BdaVertex vertices[];
};

layout(push_constant) uniform PushConstants {
    SceneUniforms sceneAddress;
    VertexStream  vertexAddress;
    uint          useStreamingData;
    float         time;
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(pc.sceneAddress.lightPos.xyz - fragPos);
    vec3 V = normalize(pc.sceneAddress.viewPos.xyz - fragPos);
    vec3 H = normalize(L + V);

    // Ambient
    vec3 ambient = 0.12 * fragColor;

    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * fragColor;

    // Specular Blinn-Phong
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = spec * vec3(1.0, 0.95, 0.85) * 0.6;

    // Rim lighting for stylish depth
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    vec3 rimColor = rim * vec3(0.3, 0.6, 1.0) * 0.4;

    vec3 result = ambient + diffuse + specular + rimColor;

    // ACES Tone Mapping
    result = (result * (2.51 * result + 0.03)) / (result * (2.43 * result + 0.59) + 0.14);
    result = clamp(result, 0.0, 1.0);

    outColor = vec4(result, 1.0);
}
