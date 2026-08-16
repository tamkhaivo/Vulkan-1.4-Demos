#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragDebug;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(1.5, 2.0, 1.0));
    vec3 V = normalize(vec3(0.0, 0.0, 3.0) - fragWorldPos);
    vec3 H = normalize(L + V);

    // Blinn-Phong Shading Model
    float ambient = 0.15;
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.5;

    // High frequency procedural pattern sensitive to fragment shading rate
    vec2 gridCoord = fragWorldPos.xy * 25.0;
    vec2 grid = abs(fract(gridCoord - 0.5) - 0.5) / fwidth(gridCoord);
    float line = min(grid.x, grid.y);
    float gridPattern = 1.0 - min(line, 1.0);

    vec3 baseCol = fragColor + vec3(0.15 * gridPattern);
    vec3 litColor = (ambient + diff) * baseCol + vec3(spec);

    outColor = vec4(litColor, 1.0);
}
