# Assignment 25 – Clustered Forward 3D Tile Lighting & Workgroup Compute

## Overview & Architectural Critique
Forward rendering fails when rendering thousands of dynamic light sources, while standard deferred rendering cannot handle MSAA or complex material transparency cleanly.

In Vulkan 1.4, **Clustered Forward Lighting** subdivides the view frustum into a 3D grid of depth-sliced spatial clusters ($X \times Y \times Z$, e.g. $16 \times 9 \times 24 = 3456$ clusters). A compute shader culls all dynamic point lights against these 3D AABB clusters using workgroup shared memory atomics, generating a compact light index list. The forward fragment shader then evaluates only the lights affecting its specific cluster, supporting 1,000+ lights in forward passes.

## Key Vulkan 1.4 Concepts
- **3D Cluster AABB Generation Compute Pass**: Calculating min/max view-space bounds per $(x, y, z)$ frustum cluster.
- **Light Culling Compute Kernel**: Parallel sphere-AABB intersection tests writing into a global Light Index SSBO and Cluster Grid SSBO (`offset`, `count`).
- **Forward Shading Cluster Lookup**: GLSL fragment shader calculates its 3D cluster coordinate from `gl_FragCoord.xy` and linear view depth $Z$, traversing only relevant lights.

## Concrete Implementation Example (GLSL Cluster Lookup & Forward Fragment Shader)

```glsl
#version 460
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;

struct PointLight {
    vec4 positionRadius; // xyz = position, w = radius
    vec4 colorIntensity; // rgb = color, w = intensity
};

struct ClusterGrid {
    uint offset;
    uint count;
};

layout(std430, set = 0, binding = 0) readonly buffer LightData { PointLight lights[]; };
layout(std430, set = 0, binding = 1) readonly buffer ClusterData { ClusterGrid clusters[]; };
layout(std430, set = 0, binding = 2) readonly buffer LightIndexList { uint lightIndices[]; };

layout(push_constant) uniform CameraParams {
    mat4 view;
    mat4 proj;
    vec4 clusterScaleBias; // For fast cluster z indexing
    uvec3 clusterDimensions; // (16, 9, 24)
} camera;

layout(location = 0) out vec4 outColor;

void main() {
    // 1. Calculate 3D Cluster Index from Screen Space & View Depth
    vec4 viewPos = camera.view * vec4(inWorldPos, 1.0);
    float viewZ = -viewPos.z;

    uint clusterX = uint(gl_FragCoord.x / (1920.0 / camera.clusterDimensions.x));
    uint clusterY = uint(gl_FragCoord.y / (1080.0 / camera.clusterDimensions.y));
    uint clusterZ = uint(max(log(viewZ) * camera.clusterScaleBias.x - camera.clusterScaleBias.y, 0.0));
    clusterZ = min(clusterZ, camera.clusterDimensions.z - 1);

    uint clusterIndex = clusterX + camera.clusterDimensions.x * (clusterY + camera.clusterDimensions.y * clusterZ);

    // 2. Traverse only lights in this cluster
    ClusterGrid cluster = clusters[clusterIndex];
    vec3 totalDiffuse = vec3(0.0);

    for (uint i = 0; i < cluster.count; ++i) {
        uint lightIdx = lightIndices[cluster.offset + i];
        PointLight light = lights[lightIdx];

        vec3 L = light.positionRadius.xyz - inWorldPos;
        float dist = length(L);
        float radius = light.positionRadius.w;

        if (dist < radius) {
            L = normalize(L);
            float nDotL = max(dot(inWorldNormal, L), 0.0);
            float attenuation = clamp(1.0 - dist / radius, 0.0, 1.0);
            attenuation *= attenuation;
            totalDiffuse += light.colorIntensity.rgb * (nDotL * attenuation * light.colorIntensity.w);
        }
    }

    outColor = vec4(totalDiffuse + vec3(0.04), 1.0);
}
```

## Acceptance Criteria
- [x] Implement compute shader constructing 3D view frustum cluster AABBs based on projection matrix.
- [x] Implement light culling compute shader testing 1,024+ point lights against 3D clusters.
- [x] Insert `VkBufferMemoryBarrier2` between light culling compute and forward graphics passes.
- [x] Render complex forward-shaded scene evaluating per-pixel dynamic clustered lights.
- [x] Maintain 60+ FPS with 1,000+ active moving lights and verify zero memory hazards.

## Directory Structure
- `src/main.cpp`: Clustered forward lighting host application.
- `shaders/cluster_aabb.comp`, `shaders/cluster_cull.comp`, `shaders/clustered_forward.frag`: Shaders.
- `CMakeLists.txt`: Build target configuration.
