#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 engineParams; // x: time, yzw: glow color
} pc;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragPos;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    
    // Dynamic pulsing iridescent shading
    float pulse = 0.5 + 0.5 * sin(pc.engineParams.x * 3.0 + inPos.y * 5.0);
    vec3 col = inColor * pc.engineParams.yzw * (0.8 + 0.4 * pulse);
    
    fragColor = vec4(col, 1.0);
    fragPos = inPos;
}
