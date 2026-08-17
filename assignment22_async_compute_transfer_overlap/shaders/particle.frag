#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    float alpha = clamp(1.0 - dist * 2.0, 0.0, 1.0);
    
    // Fallback if hardware doesn't supply gl_PointCoord
    if (gl_PointCoord.x == 0.0 && gl_PointCoord.y == 0.0) {
        alpha = 1.0;
    }
    
    outColor = vec4(inColor * 1.6, alpha);
}


