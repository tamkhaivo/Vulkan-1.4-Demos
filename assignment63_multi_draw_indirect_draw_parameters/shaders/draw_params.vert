#version 450
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 vp;
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragPos;

void main() {
    // Offset each sub-mesh instance dynamically
    float drawId = float(gl_DrawIDARB);
    float angle = drawId * 0.35;
    float radius = 0.5 + 0.3 * sin(drawId);
    vec3 offset = vec3(cos(angle) * radius, sin(angle) * radius, sin(drawId * 2.0) * 0.2);

    gl_Position = pc.vp * vec4(inPos * 0.35 + offset, 1.0);
    
    vec3 drawColor = vec3(sin(drawId * 1.5) * 0.5 + 0.5,
                          cos(drawId * 2.1) * 0.5 + 0.5,
                          sin(drawId * 3.7) * 0.5 + 0.5);
    fragColor = vec4(inColor * drawColor, 1.0);
    fragPos = inPos;
}
