#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = pc.mvp * vec4(inPos.xyz, 1.0);
    gl_PointSize = 12.0;

    // Color based on particle velocity speed & height
    float speed = length(inVel.xyz) * 0.18;
    vec3 colLow = vec3(0.1, 0.5, 1.0);   // Electric cyan-blue
    vec3 colHigh = vec3(1.0, 0.5, 0.1);  // Vivid gold-orange
    outColor = mix(colLow, colHigh, clamp(speed, 0.0, 1.0));
}

