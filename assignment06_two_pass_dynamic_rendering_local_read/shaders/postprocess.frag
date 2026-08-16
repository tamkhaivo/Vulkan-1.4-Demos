#version 450

// Vulkan 1.4 Dynamic Rendering Local Read input attachment (Attachment Index 0, Binding 0)
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inColorAttachment;

// Push constants for dynamic vignette, chromatic aberration, tone mapping, and pulse
layout(push_constant) uniform PostProcessParams {
    float time;
    float blurRadius;
    float vignetteStrength;
    uint effectMode; // 0 = Enhanced Vivid HDR Bloom/Vignette, 1 = Raw
} params;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Perform on-chip dynamic rendering local read via subpassLoad
    vec4 centerSample = subpassLoad(inColorAttachment);
    vec3 color = centerSample.rgb;

    if (params.effectMode == 0) {
        // Calculate radial distance from screen center for cinematic vignette & bloom
        vec2 uv = inUV - 0.5;
        float dist = length(uv);

        // Vignette falloff
        float vignette = smoothstep(0.85, 0.25, dist * (1.0 + params.vignetteStrength * 0.5));

        // Subtle dynamic ambient glow / chromatic dispersion calculation
        vec3 glow = vec3(
            sin(params.time * 1.5 + dist * 4.0) * 0.04 + 0.04,
            cos(params.time * 1.2 + dist * 3.0) * 0.03 + 0.03,
            sin(params.time * 2.0 + dist * 5.0) * 0.05 + 0.05
        );

        color = color * vignette + glow;

        // ACES Film Tone Mapping curve for rich aesthetics
        color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
        color = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(color, 1.0);
}
