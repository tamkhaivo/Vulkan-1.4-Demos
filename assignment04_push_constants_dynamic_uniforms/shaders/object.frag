#version 450

// Binding 1: Dynamic Material Uniform Buffer
layout(set = 0, binding = 1) uniform DynamicMaterialUBO {
    vec4 baseColor;
    vec4 ambient;
    vec4 diffuse;
    vec4 specularRoughness; // rgb = specular color, w = roughness / shininess
} material;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in flat uint fragObjectIndex;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(3.0, 5.0, 4.0) - fragWorldPos);
    vec3 V = normalize(vec3(0.0, 2.5, 6.0) - fragWorldPos);
    vec3 H = normalize(L + V);

    // Ambient
    vec3 ambient = material.ambient.rgb * material.baseColor.rgb * fragColor;

    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = material.diffuse.rgb * material.baseColor.rgb * fragColor * diff;

    // Specular (Blinn-Phong)
    float shininess = material.specularRoughness.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = material.specularRoughness.rgb * spec;

    // Subtle edge rim light for visual punch
    float rim = 1.0 - max(dot(V, N), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    vec3 rimLight = material.baseColor.rgb * rim * 0.4;

    vec3 result = ambient + diffuse + specular + rimLight;
    outColor = vec4(result, 1.0);
}
