#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

// SMPTE ST 2084 Perceptual Quantizer (PQ) tonemapping function
vec3 linearToPQ(vec3 L) {
    const float m1 = 2610.0 / 4096.0 / 4.0;
    const float m2 = 2523.0 / 4096.0 * 128.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 4096.0 * 32.0;
    const float c3 = 2392.0 / 4096.0 * 32.0;

    vec3 Y = clamp(L, 0.0, 1.0);
    vec3 Ym1 = pow(Y, vec3(m1));
    vec3 num = c1 + c2 * Ym1;
    vec3 den = 1.0 + c3 * Ym1;
    return pow(num / den, vec3(m2));
}

void main() {
    vec3 hdrColor = fragColor.rgb * 2.5; // High dynamic range radiance
    vec3 pqEncoded = linearToPQ(hdrColor * 0.4);
    outColor = vec4(pqEncoded, 1.0);
}
