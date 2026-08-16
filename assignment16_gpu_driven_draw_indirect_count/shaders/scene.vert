#version 460

struct ObjectData {
    vec4 sphereBounds;
    mat4 modelMatrix;
};

layout(std430, set = 0, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outColor;

layout(push_constant) uniform SceneCamera {
    mat4 viewProj;
} camera;

void main() {
    uint objId = gl_BaseInstance;
    ObjectData obj = objects[objId];

    vec4 worldPos = obj.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = camera.viewProj * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(obj.modelMatrix)));
    outNormal = normalize(normalMatrix * inNormal);
    outTexCoord = inTexCoord;
    outColor = vec3(0.2 + 0.8 * fract(float(objId) * 0.123), 
                    0.3 + 0.7 * fract(float(objId) * 0.456), 
                    0.4 + 0.6 * fract(float(objId) * 0.789));
}
