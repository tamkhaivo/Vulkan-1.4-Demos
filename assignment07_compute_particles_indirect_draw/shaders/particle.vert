#version 450

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

// Particle SSBO (read in vertex shader via buffer binding or SSBO)
struct Particle {
    vec4 pos; // xyz = position, w = size
    vec4 vel; // xyz = velocity, w = life
    vec4 col; // rgba
};

layout(std430, set = 0, binding = 1) readonly buffer ParticleBuffer {
    Particle particles[];
};

// Billboard Quad Vertices: 4 vertices for a quad [-1, 1]
const vec2 quadOffsets[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

const vec2 quadUVs[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main() {
    uint particleIndex = gl_InstanceIndex;
    Particle p = particles[particleIndex];

    vec3 particleCenter = p.pos.xyz;
    float particleSize = p.pos.w;

    // View-aligned billboard orientation: extract camera Right and Up vectors from View matrix
    vec3 cameraRight = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 cameraUp    = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);

    vec2 offset = quadOffsets[gl_VertexIndex] * particleSize;
    vec3 vertexWorldPos = particleCenter + (cameraRight * offset.x) + (cameraUp * offset.y);

    gl_Position = ubo.proj * ubo.view * vec4(vertexWorldPos, 1.0);

    outColor = p.col;
    outUV = quadUVs[gl_VertexIndex];
}
