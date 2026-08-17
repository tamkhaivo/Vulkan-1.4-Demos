# Assignment 73 – Hardware Fragment Barycentrics & Analytic Wireframe Anti-Aliasing (VK_KHR_fragment_shader_barycentric)

## Overview & Architectural Critique
Traditional wireframe rendering requires duplicate geometry draw calls, geometry shaders, or specialized per-vertex 3D attributes that degrade GPU cache locality and vertex shader efficiency.

**Assignment 73** leverages **`VK_KHR_fragment_shader_barycentric`**:
1. **Direct Hardware Barycentric Coordinates**: Injects `gl_BaryCoordKHR` directly into fragment shaders from standard indexed triangle meshes.
2. **Analytic Edge Anti-Aliasing**: Evaluates screen-space partial derivative rates (`fwidth(bary)`) to compute analytic pixel-perfect distance to triangle edges.
3. **Single-Pass CAD / Technical Visualization**: Delivers crystal-clear, anti-aliased wireframe lines overlaid on shaded geometry with zero geometry duplication.

## Key Vulkan 1.4 Concepts
- **`VK_KHR_fragment_shader_barycentric`**: `VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR.fragmentShaderBarycentric`.
- **`gl_BaryCoordKHR`**: Hardware barycentric weights $(u, v, w)$ normalized to $u + v + w = 1.0$.
- **Dynamic Rendering**: Single-pass color and depth attachment rendering.

## Acceptance Criteria
- [x] Enable `VK_KHR_fragment_shader_barycentric` extension and physical device features.
- [x] Write fragment shader evaluating screen-space derivatives of barycentrics for analytic anti-aliasing.
- [x] Render complex 3D geometric mesh with anti-aliased wireframe overlay in a single draw call.
- [x] Clean execution with 0 Vulkan validation errors.

## Directory Structure
- `src/main.cpp`: Barycentrics rendering pipeline host application.
- `shaders/bary_wireframe.vert`, `shaders/bary_wireframe.frag`: Barycentric fragment shader suite.
- `CMakeLists.txt`: Build target configuration.
