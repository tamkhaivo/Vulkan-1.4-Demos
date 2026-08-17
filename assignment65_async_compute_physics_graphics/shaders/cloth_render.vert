#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 clothParams; // x: time, yzw: color tint
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragPos;

void main() {
    // Dynamic cloth ripple wave
    float wave = sin(inPos.x * 6.0 + pc.clothParams.x * 4.0) * cos(inPos.z * 6.0 + pc.clothParams.x * 4.0) * 0.15;
    vec3 animatedPos = inPos + vec3(0.0, wave, 0.0);

    gl_Position = pc.mvp * vec4(animatedPos, 1.0);
    fragColor = vec4(inColor * pc.clothParams.yzw, 1.0);
    fragPos = animatedPos;
}
