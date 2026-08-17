#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float playbackTime;

layout(location = 0) out vec4 outColor;

void main() {
    // Hardware AV1 video decode stream emulation (YCbCr 4:2:0 matrix conversion)
    vec2 uv = fragUV;
    float t = playbackTime * 1.5;
    
    // Generate synthetic YCbCr planes
    float Y = 0.5 + 0.4 * sin(uv.x * 16.0 + t) * cos(uv.y * 12.0 - t);
    float Cb = 0.5 + 0.3 * sin(uv.x * 6.0 - t * 0.8);
    float Cr = 0.5 + 0.3 * cos(uv.y * 6.0 + t * 0.8);

    // ITU-R BT.709 conversion matrix
    float r = Y + 1.5748 * (Cr - 0.5);
    float g = Y - 0.1873 * (Cb - 0.5) - 0.4681 * (Cr - 0.5);
    float b = Y + 1.8556 * (Cb - 0.5);

    // AV1 film grain emulation
    float grain = fract(sin(dot(uv + vec2(t * 0.01), vec2(12.9898, 78.233))) * 43758.5453) * 0.05;

    outColor = vec4(clamp(vec3(r, g, b) + grain, 0.0, 1.0), 1.0);
}
