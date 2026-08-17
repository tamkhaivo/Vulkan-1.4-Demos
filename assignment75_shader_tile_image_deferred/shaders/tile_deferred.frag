#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFinalColor;

layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;

layout(push_constant) uniform LightingConstants {
    vec4 lightPos[4];
    vec4 lightColor[4];
    vec4 viewPos;
    int numLights;
} pc;

void main() {
    vec4 albedo = texture(gAlbedo, inUV);
    vec3 normal = texture(gNormal, inUV).xyz;

    if (length(normal) < 0.1) {
        // Background sky
        outFinalColor = vec4(0.02, 0.03, 0.06, 1.0);
        return;
    }

    vec3 N = normalize(normal);
    vec3 ambient = albedo.rgb * 0.15;
    vec3 totalDiffuse = vec3(0.0);

    for (int i = 0; i < pc.numLights; ++i) {
        vec3 L = normalize(pc.lightPos[i].xyz);
        float NdotL = max(dot(N, L), 0.0);
        totalDiffuse += albedo.rgb * pc.lightColor[i].rgb * NdotL;
    }

    outFinalColor = vec4(ambient + totalDiffuse, 1.0);
}
