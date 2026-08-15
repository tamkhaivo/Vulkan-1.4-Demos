#version 450

// Binding 0: Camera / Scene UBO (Static Uniform Buffer)
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
} scene;

// Push Constants: Per-object model transform & object ID / timing
layout(push_constant) uniform PushConstants {
    mat4 model;
    float time;
    uint objectIndex;
} pushConsts;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out flat uint fragObjectIndex;

void main() {
    vec4 worldPos = pushConsts.model * vec4(inPosition, 1.0);
    gl_Position = scene.proj * scene.view * worldPos;
    
    // Normal transform (assuming uniform scale or rotation)
    mat3 normalMatrix = mat3(pushConsts.model);
    fragNormal = normalize(normalMatrix * inNormal);
    
    fragColor = inColor;
    fragWorldPos = worldPos.xyz;
    fragObjectIndex = pushConsts.objectIndex;
}
