#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec4 fragViewPos;

struct PointLight {
    vec4 positionRadius; // xyz: pos, w: radius
    vec4 colorIntensity; // rgb: col, w: intensity
};

layout(std430, set = 0, binding = 0) readonly buffer LightBuffer {
    PointLight lights[];
};

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    mat4 view;
    uint totalLights;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragWorldNormal);
    vec3 ambient = vec3(0.04) * fragColor;
    vec3 totalDiffuse = vec3(0.0);

    for (uint i = 0; i < pc.totalLights; ++i) {
        PointLight light = lights[i];
        vec3 lightPos = light.positionRadius.xyz;
        float radius = light.positionRadius.w;
        vec3 lightColor = light.colorIntensity.rgb;
        float intensity = light.colorIntensity.w;

        vec3 L = lightPos - fragWorldPos;
        float dist = length(L);

        if (dist < radius) {
            L = normalize(L);
            float nDotL = max(dot(N, L), 0.0);
            float att = clamp(1.0 - (dist / radius), 0.0, 1.0);
            att *= att; // Quadratic falloff
            totalDiffuse += lightColor * (nDotL * att * intensity) * fragColor;
        }
    }

    outColor = vec4(ambient + totalDiffuse, 1.0);
}
