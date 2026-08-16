#version 460

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outFragColor;

void main() {
    vec3 N = normalize(inNormal);
    if (!gl_FrontFacing) {
        N = -N;
    }

    vec3 lightDir = normalize(vec3(0.6, 1.2, 0.7));
    float diff = max(dot(N, lightDir), 0.0);
    float ambient = 0.35;

    vec3 viewDir = normalize(vec3(0.0, 1.5, 2.0) - inWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(N, halfDir), 0.0), 32.0) * 0.40;

    // Subtle rim lighting for depth definition
    float rim = 1.0 - max(dot(viewDir, N), 0.0);
    rim = pow(rim, 3.0) * 0.20;

    vec3 finalColor = inColor * (diff + ambient + rim) + vec3(spec);
    outFragColor = vec4(finalColor, 1.0);
}
