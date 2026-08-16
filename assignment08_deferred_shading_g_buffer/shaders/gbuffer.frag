#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

// Multiple Render Targets (MRT) G-Buffer outputs
layout(location = 0) out vec4 outPosition; // RGBA16_SFLOAT or RGBA32_SFLOAT: xyz = World Position, w = linear depth
layout(location = 1) out vec4 outNormal;   // RGBA16_SFLOAT: xyz = Normal, w = unused
layout(location = 2) out vec4 outAlbedo;   // RGBA8_UNORM: rgb = Albedo, a = specular power / roughness

void main() {
    outPosition = vec4(fragWorldPos, gl_FragCoord.z);
    outNormal = vec4(normalize(fragNormal), 1.0);
    outAlbedo = vec4(fragColor, 1.0);
}
