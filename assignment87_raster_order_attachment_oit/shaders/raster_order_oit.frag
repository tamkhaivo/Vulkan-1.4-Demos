#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 blendParams; // x: time, y: layerCount
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(2.0, 3.0, 2.0) - fragWorldPos);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    // Dynamic Fresnel reflection
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    // Programmable raster order attachment blending simulation
    vec3 glassColor = fragColor.rgb * (diff * 0.6 + 0.2) + vec3(spec * 0.8) + vec3(fresnel * 0.4);
    float alpha = clamp(fragColor.a + fresnel * 0.3, 0.2, 0.95);

    outColor = vec4(glassColor, alpha);
}
