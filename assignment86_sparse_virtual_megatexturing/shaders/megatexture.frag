#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 streamParams; // x: time, y: residentPageCount
} pc;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(2.0, 3.0, 2.0) - fragWorldPos);
    float diff = max(dot(N, L), 0.0);

    // Multi-layer virtual megatexture procedural simulation (16K virtual space)
    vec2 tileUV = fract(fragUV * 16.0);
    vec2 pageCoord = floor(fragUV * 16.0);

    // Checkerboard tile border
    float border = (tileUV.x < 0.04 || tileUV.x > 0.96 || tileUV.y < 0.04 || tileUV.y > 0.96) ? 0.3 : 1.0;

    // Dynamic procedural multi-layer terrain texture (Rock, Sand, Grass)
    vec3 rockColor = vec3(0.5, 0.45, 0.4);
    vec3 grassColor = vec3(0.2, 0.65, 0.25);
    vec3 sandColor = vec3(0.8, 0.7, 0.4);

    float blend = sin(fragUV.x * 30.0 + pc.streamParams.x) * cos(fragUV.y * 30.0);
    vec3 surface = mix(sandColor, (blend > 0.0 ? grassColor : rockColor), 0.7);

    vec3 shaded = (vec3(0.15) + diff * 0.85) * surface * border;
    outColor = vec4(shaded, 1.0);
}
