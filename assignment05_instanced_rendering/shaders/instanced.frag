#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in flat uint fragInstanceID;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 lightPos = vec3(10.0, 20.0, 15.0);
    vec3 L = normalize(lightPos - fragWorldPos);
    vec3 viewPos = vec3(0.0, 12.0, 24.0);
    vec3 V = normalize(viewPos - fragWorldPos);
    vec3 H = normalize(L + V);

    // Ambient
    vec3 ambient = 0.22 * fragColor.rgb;

    // Diffuse (Lambert)
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = 0.75 * diff * fragColor.rgb;

    // Specular (Blinn-Phong)
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specular = vec3(0.8) * spec;

    // Grid wireframe / edge accent on cube faces
    vec2 edgeUV = abs(fragTexCoord - 0.5) * 2.0;
    float border = max(edgeUV.x, edgeUV.y);
    float edgeFactor = smoothstep(0.88, 0.98, border);
    vec3 edgeGlow = fragColor.rgb * edgeFactor * 0.6;

    // Fresnel / Rim Lighting
    float rim = 1.0 - max(dot(V, N), 0.0);
    rim = smoothstep(0.65, 1.0, rim);
    vec3 rimColor = fragColor.rgb * rim * 0.45;

    vec3 finalColor = ambient + diffuse + specular + edgeGlow + rimColor;
    outColor = vec4(finalColor, 1.0);
}
