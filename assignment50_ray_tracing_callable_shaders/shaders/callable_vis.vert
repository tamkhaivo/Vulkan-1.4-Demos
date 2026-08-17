#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 materialParams; // x: materialId, y: roughness, zw: unused
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;
layout(location = 2) out flat uint fragMatId;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    fragNormal = inNormal;
    fragPos = inPos;
    fragMatId = uint(pc.materialParams.x);
}
