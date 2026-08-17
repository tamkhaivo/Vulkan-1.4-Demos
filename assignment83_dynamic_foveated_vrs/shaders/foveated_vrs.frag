#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 gazePos; // xy: gaze center in screen uv, z: time
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(1.5, 2.0, 2.5) - fragWorldPos);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec2 screenUV = gl_FragCoord.xy / vec2(800.0, 600.0);
    vec2 gazeCenter = pc.gazePos.xy;
    float dist = distance(screenUV, gazeCenter);

    // Variable Rate Shading density overlay
    vec3 rateTint = vec3(1.0);
    if (dist < 0.2) {
        rateTint = vec3(0.4, 1.0, 0.4); // 1x1 Pinpoint Fovea (Green tint)
    } else if (dist < 0.4) {
        rateTint = vec3(1.0, 0.8, 0.2); // 2x2 Intermediate (Yellow tint)
    } else {
        rateTint = vec3(1.0, 0.3, 0.3); // 4x4 Coarse Periphery (Red tint)
    }

    // Reticle center point
    if (dist < 0.01) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    vec3 shaded = (vec3(0.1) + diff * 0.7 + spec * 0.4) * fragColor * rateTint;
    outColor = vec4(shaded, 1.0);
}
