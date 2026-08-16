#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

void main() {
    // Distance from center of billboard quad for soft spherical glow particle
    vec2 coord = inUV * 2.0 - 1.0;
    float distSq = dot(coord, coord);
    if (distSq > 1.0) {
        discard;
    }

    // Soft Gaussian-like radial glow dropoff
    float intensity = exp(-distSq * 3.5);
    
    // Core brightness boost
    float core = smoothstep(0.4, 0.0, distSq);
    vec3 glowColor = inColor.rgb + vec3(0.5, 0.5, 0.6) * core;

    float alpha = inColor.a * intensity;
    if (alpha < 0.01) {
        discard;
    }

    outFragColor = vec4(glowColor, alpha);
}
