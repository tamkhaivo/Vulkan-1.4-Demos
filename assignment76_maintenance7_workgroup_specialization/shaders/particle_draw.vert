#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform DrawPushConstants {
    layout(offset = 16) mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPos.xyz * 0.05, 1.0);
    gl_PointSize = 3.5;
    fragColor = inColor;
}
