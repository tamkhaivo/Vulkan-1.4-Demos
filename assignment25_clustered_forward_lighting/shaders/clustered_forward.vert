#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    mat4 view;
    uint totalLights;
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec4 fragViewPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPos, 1.0);
    gl_Position = pc.mvp * vec4(inPos, 1.0);

    fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(pc.model) * inNormal;
    fragColor = inColor;
    fragViewPos = pc.view * worldPos;
}
