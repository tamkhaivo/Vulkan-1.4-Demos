#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(std430, set = 0, binding = 1) readonly buffer VisibilityBuffer {
    uint64_t pixels[];
};

layout(push_constant) uniform ResolvePushConstants {
    layout(offset = 16) int screenWidth;
    int screenHeight;
    float time;
};

// Generates distinct colors per micro-polygon/cluster ID
vec3 getClusterColor(uint id) {
    if (id == 0xFFFFFFFFu) {
        // Background sky gradient
        return mix(vec3(0.02, 0.03, 0.06), vec3(0.08, 0.12, 0.22), inUV.y);
    }
    float hue = fract(float(id) * 0.618033988749895);
    vec3 c = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return mix(vec3(1.0), c, 0.85);
}

void main() {
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    if (pixelCoord.x >= screenWidth || pixelCoord.y >= screenHeight) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    uint pixelIdx = uint(pixelCoord.y * screenWidth + pixelCoord.x);
    uint64_t val = pixels[pixelIdx];

    uint depthUint = uint(val >> 32);
    uint triId = uint(val & 0xFFFFFFFFu);

    if (triId == 0xFFFFFFFFu) {
        outColor = vec4(getClusterColor(0xFFFFFFFFu), 1.0);
        return;
    }

    float depth = uintBitsToFloat(depthUint);
    vec3 baseColor = getClusterColor(triId);

    // Apply depth-based shading & ambient occlusion effect
    float depthShade = clamp((1.0 - depth) * 1.5, 0.2, 1.0);
    vec3 finalColor = baseColor * depthShade;

    // Grid wireframe highlight for micro-triangles
    outColor = vec4(finalColor, 1.0);
}
