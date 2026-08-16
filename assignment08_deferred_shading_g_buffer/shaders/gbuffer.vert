#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPos, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(transpose(inverse(ubo.model))) * inNormal);
    fragColor = inColor;
    gl_Position = ubo.proj * ubo.view * worldPos;
}
