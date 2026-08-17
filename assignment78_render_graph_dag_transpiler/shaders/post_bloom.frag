#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneColorTex;

layout(push_constant) uniform PostConstants {
    float bloomIntensity;
    float time;
} pc;

void main() {
    vec4 baseColor = texture(sceneColorTex, inUV);

    // Simple analytical glow / tone mapping
    vec3 glow = max(baseColor.rgb - vec3(0.3), vec3(0.0)) * pc.bloomIntensity;
    vec3 mapped = (baseColor.rgb + glow);
    mapped = mapped / (mapped + vec3(1.0)); // Reinhard tone mapping

    outColor = vec4(mapped, 1.0);
}
