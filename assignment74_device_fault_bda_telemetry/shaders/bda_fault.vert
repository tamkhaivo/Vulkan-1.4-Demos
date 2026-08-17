#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 pos;
    vec3 color;
};

layout(buffer_reference, scalar) readonly buffer VertexBufferRef {
    Vertex vertices[];
};

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    VertexBufferRef vertexBufferAddress;
} pc;

void main() {
    Vertex v = pc.vertexBufferAddress.vertices[gl_VertexIndex];
    gl_Position = pc.mvp * vec4(v.pos, 1.0);
    fragColor = v.color;
}
