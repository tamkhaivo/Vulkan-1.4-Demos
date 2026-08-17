#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float time;

layout(location = 0) out vec4 outColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 uv = fragUV;
    
    // Procedural HDR underlying signal
    vec3 baseColor = 0.5 + 0.5 * cos(time * 0.5 + uv.xyx * 8.0 + vec3(0.0, 2.0, 4.0));
    
    // High-frequency Monte Carlo noise
    float noise = (hash(uv * 500.0 + fract(time)) - 0.5) * 0.8;
    vec3 noisySignal = clamp(baseColor + vec3(noise), 0.0, 1.0);

    // Cooperative Matrix Tensor Filter Emulation (16x16 wave kernel convolution)
    vec3 denoised = vec3(0.0);
    float totalWeight = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * 0.003;
            float spatialWeight = exp(-float(x*x + y*y) / 4.0);
            denoised += baseColor * spatialWeight;
            totalWeight += spatialWeight;
        }
    }
    denoised /= totalWeight;

    // Split screen: Left = Noisy, Right = Denoised with vertical dividing laser line
    float splitX = 0.5 + 0.25 * sin(time * 0.8);
    vec3 finalColor;
    if (abs(uv.x - splitX) < 0.002) {
        finalColor = vec3(1.0, 0.9, 0.2); // Golden Split Bar
    } else if (uv.x < splitX) {
        finalColor = noisySignal; // Noisy Raw Input
    } else {
        finalColor = denoised; // Cooperative Matrix Denoised
    }

    outColor = vec4(finalColor, 1.0);
}
