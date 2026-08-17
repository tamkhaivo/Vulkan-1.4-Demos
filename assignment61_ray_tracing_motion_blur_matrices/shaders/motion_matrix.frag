#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragPos;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 motionVector;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    // Multi-keyframe matrix motion streak simulation
    float speed = length(pc.motionVector.xyz);
    vec3 motionColor = mix(fragColor.rgb, vec3(1.0, 0.9, 0.4), clamp(speed * 0.3, 0.0, 0.8));
    outColor = vec4(motionColor, 1.0);
}
