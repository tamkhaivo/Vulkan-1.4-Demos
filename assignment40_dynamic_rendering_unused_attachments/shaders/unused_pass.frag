#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

// 3 Color attachment output locations in the compiled PSO
layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;

void main() {
    vec3 N = normalize(fragNormal);
    outAlbedo = vec4(fragColor, 1.0);
    outNormal = vec4(N * 0.5 + 0.5, 1.0);
    outPosition = vec4(fragWorldPos, 1.0);
}
