#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 dispParam; // x: dispAmount, y: time
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragPos;

void main() {
    // Dynamic vertex displacement simulation along normal
    float wave = sin(inPos.x * 8.0 + pc.dispParam.y * 2.0) * cos(inPos.z * 8.0 + pc.dispParam.y * 2.0);
    vec3 displacedPos = inPos + inNormal * (wave * pc.dispParam.x);

    gl_Position = pc.mvp * vec4(displacedPos, 1.0);
    fragNormal = inNormal;
    fragPos = displacedPos;
}
