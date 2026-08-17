#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float time;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = fragUV;
    float t = time * 2.0;

    // CPU-to-GPU Host-Image-Copy procedural texture pattern
    float pattern = sin(uv.x * 20.0 + t) * cos(uv.y * 20.0 - t);
    vec3 col = vec3(0.5 + 0.5 * sin(t + uv.xyx * 5.0 + vec3(0, 2, 4)));

    // ReBAR direct memory blit grid overlay
    vec2 grid = fract(uv * 10.0);
    float border = (grid.x < 0.05 || grid.y < 0.05) ? 0.3 : 1.0;

    outColor = vec4(col * (0.8 + 0.2 * pattern) * border, 1.0);
}
