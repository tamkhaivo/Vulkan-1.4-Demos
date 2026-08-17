#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform StereoPushConstants {
    mat4 leftMVP;
    mat4 rightMVP;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    mat4 mvp = (gl_ViewIndex == 0) ? pc.leftMVP : pc.rightMVP;
    gl_Position = mvp * vec4(inPos, 1.0);

    vec3 eyeTint = (gl_ViewIndex == 0) ? vec3(0.2, 0.8, 1.0) : vec3(1.0, 0.3, 0.7);
    fragColor = inColor * eyeTint;
}
