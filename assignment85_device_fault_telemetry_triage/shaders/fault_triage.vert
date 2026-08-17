#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec3 color;
};

layout(buffer_reference, scalar) readonly buffer VertexBufferRef {
    Vertex vertices[];
};

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragWorldPos;

layout(push_constant, scalar) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    uint64_t vertexBufferAddress;
    float time;
} pc;

void main() {
    VertexBufferRef vertexBuffer = VertexBufferRef(pc.vertexBufferAddress);
    Vertex v = vertexBuffer.vertices[gl_VertexIndex];

    vec4 worldPos = pc.model * vec4(v.pos, 1.0);
    gl_Position = pc.mvp * vec4(v.pos, 1.0);

    fragNormal = mat3(pc.model) * v.normal;
    fragColor = v.color;
    fragWorldPos = worldPos.xyz;
}
