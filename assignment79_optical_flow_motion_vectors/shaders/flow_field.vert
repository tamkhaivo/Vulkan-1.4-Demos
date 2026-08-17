#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 curClipPos;
layout(location = 2) out vec4 prevClipPos;

layout(push_constant) uniform MotionPushConstants {
    mat4 curMVP;
    mat4 prevMVP;
    mat4 curModel;
} pc;

void main() {
    curClipPos = pc.curMVP * vec4(inPos, 1.0);
    prevClipPos = pc.prevMVP * vec4(inPos, 1.0);
    gl_Position = curClipPos;
    fragNormal = mat3(pc.curModel) * inNormal;
}
