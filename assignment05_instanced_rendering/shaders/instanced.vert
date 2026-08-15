#version 450

// Binding 0: Camera / Scene UBO
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
} scene;

// Push Constants: Global time / animation parameters
layout(push_constant) uniform PushConstants {
    float time;
    uint totalInstances;
} pushConsts;

// Per-Vertex Attributes (Binding 0, Vertex Input Rate = VERTEX)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Per-Instance Attributes (Binding 1, Vertex Input Rate = INSTANCE, Divisor = 1)
layout(location = 3) in vec3 inInstPos;
layout(location = 4) in vec4 inInstRot;     // axis-angle (xyz = axis, w = angle)
layout(location = 5) in vec3 inInstScale;
layout(location = 6) in vec4 inInstColor;
layout(location = 7) in vec4 inInstCustom;    // x = orbit radius, y = orbit speed, z = pulse phase, w = material ID

// Outputs to Fragment Shader
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out flat uint fragInstanceID;

// Helper: Rotate vector by axis-angle
vec3 rotateAxis(vec3 v, vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    mat3 rot = mat3(
        oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
        oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
        oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
    );
    return rot * v;
}

void main() {
    float t = pushConsts.time;
    
    // Dynamic instance animation based on instance parameters
    float dynamicAngle = inInstRot.w + t * inInstCustom.y;
    
    // Wave oscillation / hover effect
    vec3 animatedPos = inInstPos;
    animatedPos.y += sin(t * 2.5 + inInstCustom.z) * 0.35;
    
    // Scale pulsation
    float pulse = 1.0 + 0.15 * sin(t * 3.0 + inInstCustom.z);
    vec3 scaledPos = inPosition * (inInstScale * pulse);
    
    // Apply instance rotation
    vec3 rotatedPos = rotateAxis(scaledPos, inInstRot.xyz, dynamicAngle);
    vec3 worldPos = rotatedPos + animatedPos;
    
    // Transform normal
    vec3 rotatedNormal = rotateAxis(inNormal, inInstRot.xyz, dynamicAngle);
    
    gl_Position = scene.proj * scene.view * vec4(worldPos, 1.0);
    
    fragNormal = normalize(rotatedNormal);
    fragColor = inInstColor;
    fragWorldPos = worldPos;
    fragTexCoord = inTexCoord;
    fragInstanceID = gl_InstanceIndex;
}
