#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) flat in uint fragTexIndex;

// Unbounded bindless texture array
layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texSample = texture(globalTextures[nonuniformEXT(fragTexIndex)], fragTexCoord);
    outColor = texSample * vec4(fragColor, 1.0);
}
