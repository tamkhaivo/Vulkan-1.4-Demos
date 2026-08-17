#version 460
#extension GL_EXT_fragment_shader_barycentric : require

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    vec4 wireColor;
    vec4 surfaceColor;
    float lineWidth;
} pc;

void main() {
    // gl_BaryCoordEXT contains hardware barycentric coordinates (u, v, w)
    vec3 bary = gl_BaryCoordEXT;

    // Screen-space partial derivative rates for pixel-width scaling
    vec3 dBary = fwidth(bary);
    vec3 edgeDist = bary / max(dBary, vec3(1e-5));
    float minEdgeDist = min(min(edgeDist.x, edgeDist.y), edgeDist.z);

    // Analytic smoothstep edge anti-aliasing
    float wireFactor = 1.0 - smoothstep(pc.lineWidth - 1.0, pc.lineWidth + 1.0, minEdgeDist);

    // Diffuse directional shading
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.5, 1.0, 0.7));
    float NdotL = max(dot(N, L), 0.2);

    vec3 shadedSurface = pc.surfaceColor.rgb * NdotL;
    vec3 finalColor = mix(shadedSurface, pc.wireColor.rgb, wireFactor);

    outColor = vec4(finalColor, 1.0);
}
