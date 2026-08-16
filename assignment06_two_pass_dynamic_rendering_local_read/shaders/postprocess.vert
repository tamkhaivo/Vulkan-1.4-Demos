#version 450

layout(location = 0) out vec2 outUV;

void main() {
    // Generate fullscreen triangle covering normalized device coordinates [-1, 1]
    // Vertex 0: (-1, -1), UV (0, 0)
    // Vertex 1: ( 3, -1), UV (2, 0)
    // Vertex 2: (-1,  3), UV (0, 2)
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
