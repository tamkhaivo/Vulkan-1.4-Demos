#version 450

// Dynamic Rendering Local Read input attachments
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inPosition;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inNormal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inAlbedo;

struct PointLight {
    vec4 position; // xyz = pos, w = radius
    vec4 color;    // rgb = color, w = intensity
};

layout(std140, set = 0, binding = 3) uniform LightUBO {
    vec4 viewPos;
    PointLight lights[6];
    int lightCount;
    int displayMode; // 0 = Full Deferred Lighting, 1 = Position, 2 = Normal, 3 = Albedo
} ubo;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFinalColor;

void main() {
    // Perform on-chip dynamic rendering local read via subpassLoad
    vec4 posSample = subpassLoad(inPosition);
    vec4 normSample = subpassLoad(inNormal);
    vec4 albedoSample = subpassLoad(inAlbedo);

    vec3 fragPos = posSample.xyz;
    vec3 N = normalize(normSample.xyz);
    vec3 albedo = albedoSample.rgb;

    // If background / empty space (Normal length near zero)
    if (length(normSample.xyz) < 0.1) {
        outFinalColor = vec4(0.02, 0.03, 0.06, 1.0); // Deep Obsidian background
        return;
    }

    // Diagnostic Visualization Modes
    if (ubo.displayMode == 1) {
        outFinalColor = vec4(fract(fragPos), 1.0);
        return;
    } else if (ubo.displayMode == 2) {
        outFinalColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    } else if (ubo.displayMode == 3) {
        outFinalColor = vec4(albedo, 1.0);
        return;
    }

    // Full Deferred Multi-Point-Light Accumulation
    vec3 V = normalize(ubo.viewPos.xyz - fragPos);
    vec3 ambient = vec3(0.04) * albedo;
    vec3 lighting = ambient;

    for (int i = 0; i < ubo.lightCount; ++i) {
        vec3 lightVec = ubo.lights[i].position.xyz - fragPos;
        float dist = length(lightVec);
        float radius = ubo.lights[i].position.w;

        if (dist < radius) {
            vec3 L = normalize(lightVec);
            vec3 H = normalize(L + V);

            // Attenuation: smooth quadratic falloff
            float atten = clamp(1.0 - (dist / radius), 0.0, 1.0);
            atten = atten * atten;

            // Diffuse
            float NdotL = max(dot(N, L), 0.0);
            vec3 diff = NdotL * albedo * ubo.lights[i].color.rgb * ubo.lights[i].color.w;

            // Specular (Blinn-Phong)
            float NdotH = max(dot(N, H), 0.0);
            float spec = pow(NdotH, 32.0);
            vec3 specular = spec * ubo.lights[i].color.rgb * ubo.lights[i].color.w * 0.5;

            lighting += (diff + specular) * atten;
        }
    }

    // ACES Tone Mapping
    vec3 color = (lighting * (2.51 * lighting + 0.03)) / (lighting * (2.43 * lighting + 0.59) + 0.14);
    color = clamp(color, 0.0, 1.0);

    outFinalColor = vec4(color, 1.0);
}
