#version 450

layout(location = 0) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D sparseTexture;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tileParams; // x: time, y: lodLevel, z: debugResidency, w: unused
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(sparseTexture, fragUV);

    // Draw tile grid lines overlay to clearly show the 64KB sparse tile boundaries
    vec2 tileGrid = abs(fract(fragUV * 8.0 - 0.5) - 0.5) / fwidth(fragUV * 8.0);
    float line = min(tileGrid.x, tileGrid.y);
    float gridFactor = 1.0 - min(line, 1.0);

    // Blend sparse virtual texture with tile boundaries and vibrant coloring
    vec3 finalColor = mix(texColor.rgb, vec3(0.0, 1.0, 0.8), gridFactor * 0.6);
    outColor = vec4(finalColor, 1.0);
}
