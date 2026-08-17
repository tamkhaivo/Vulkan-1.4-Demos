#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;
layout(location = 2) in flat uint fragMatId;

layout(location = 0) out vec4 outColor;

// Procedural BRDF Callable Emulation Functions
vec3 evalDiffuseBRDF(vec3 N, vec3 L) {
    float NdotL = max(dot(N, L), 0.0);
    return vec3(0.9, 0.3, 0.2) * (NdotL + 0.15);
}

vec3 evalGGXGoldBRDF(vec3 N, vec3 V, vec3 L) {
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 F0 = vec3(1.00, 0.78, 0.34); // Gold
    vec3 fresnel = F0 + (vec3(1.0) - F0) * pow(1.0 - max(dot(V, H), 0.0), 5.0);
    float spec = pow(NdotH, 32.0);
    return fresnel * spec + F0 * 0.2 * NdotL;
}

vec3 evalClearcoatBRDF(vec3 N, vec3 V, vec3 L) {
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 baseCol = vec3(0.1, 0.4, 0.95);
    float clearcoatSpec = pow(NdotH, 64.0) * 1.5;
    return baseCol * NdotL + vec3(clearcoatSpec);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(vec3(0.0, 0.0, 2.5) - fragPos);
    vec3 L = normalize(vec3(1.0, 1.5, 1.2));

    vec3 brdf = vec3(0.0);
    if (fragMatId == 0) {
        brdf = evalDiffuseBRDF(N, L);
    } else if (fragMatId == 1) {
        brdf = evalGGXGoldBRDF(N, V, L);
    } else {
        brdf = evalClearcoatBRDF(N, V, L);
    }

    outColor = vec4(brdf, 1.0);
}
