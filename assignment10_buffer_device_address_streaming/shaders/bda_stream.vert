#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;

// Define BDA reference struct for Scene Uniforms
layout(buffer_reference, scalar) readonly buffer SceneUniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightPos;
    vec4 viewPos;
};

// Define BDA reference struct for Vertex Streams (Direct pointer memory access)
struct BdaVertex {
    vec4 pos;      // xyz = pos, w = u
    vec4 color;    // rgba
    vec4 normal;   // xyz = normal, w = v
};

layout(buffer_reference, scalar) readonly buffer VertexStream {
    BdaVertex vertices[];
};

// Push Constants delivering raw 64-bit GPU device addresses
layout(push_constant) uniform PushConstants {
    SceneUniforms sceneAddress;    // 64-bit uint64_t pointer to UBO
    VertexStream  vertexAddress;   // 64-bit uint64_t pointer to streamed vertex buffer
    uint          useStreamingData; // 1 = fetch from BDA VertexStream, 0 = use traditional vertex attributes
    float         time;
} pc;

void main() {
    mat4 model = pc.sceneAddress.model;
    mat4 view  = pc.sceneAddress.view;
    mat4 proj  = pc.sceneAddress.proj;

    vec3 currentPos;
    vec3 currentColor;
    vec3 currentNormal;

    if (pc.useStreamingData == 1) {
        // Read directly from zero-copy streamed GPU memory via BDA raw pointer dereference
        BdaVertex v = pc.vertexAddress.vertices[gl_VertexIndex];
        currentPos = v.pos.xyz;
        currentColor = v.color.rgb;
        currentNormal = v.normal.xyz;
    } else {
        // Fallback to standard vertex input attributes
        currentPos = inPos;
        currentColor = inColor;
        currentNormal = inNormal;
    }

    vec4 worldPos = model * vec4(currentPos, 1.0);
    gl_Position = proj * view * worldPos;

    fragPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(model))) * currentNormal;
    fragColor = currentColor;
}
