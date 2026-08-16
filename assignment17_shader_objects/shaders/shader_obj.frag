#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    layout(offset = 16) vec4 tintColor;
} push;

void main() {
    outColor = vec4(fragColor * push.tintColor.rgb, 1.0);
}
