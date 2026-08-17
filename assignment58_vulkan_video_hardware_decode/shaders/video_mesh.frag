#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float playbackTime;

layout(location = 0) out vec4 outColor;

void main() {
    // Hardware video decoding playback simulation (dynamic live test pattern with scanlines)
    vec2 uv = fragUV;
    float scanline = sin(uv.y * 120.0 + playbackTime * 10.0) * 0.08;

    // Moving color bars
    float bar = fract(uv.x * 4.0 + playbackTime * 0.5);
    vec3 color = vec3(0.5 + 0.5 * sin(bar * 6.28), 0.5 + 0.5 * cos(bar * 6.28), 0.8);

    outColor = vec4(color + vec3(scanline), 1.0);
}
