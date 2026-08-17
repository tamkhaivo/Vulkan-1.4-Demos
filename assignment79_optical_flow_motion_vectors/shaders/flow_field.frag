#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec4 curClipPos;
layout(location = 2) in vec4 prevClipPos;

layout(location = 0) out vec4 outMotionColor;

// Converts 2D motion direction and magnitude to vibrant color
vec3 motionToColor(vec2 motion) {
    float angle = atan(motion.y, motion.x);
    float speed = length(motion) * 15.0; // amplify for clear visual feedback

    float hue = (angle + 3.14159265) / (2.0 * 3.14159265);
    vec3 c = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c * clamp(speed, 0.2, 1.0);
}

void main() {
    vec2 curNDC = curClipPos.xy / curClipPos.w;
    vec2 prevNDC = prevClipPos.xy / prevClipPos.w;
    vec2 motionVector = (curNDC - prevNDC) * 0.5; // screen-space velocity

    vec3 col = motionToColor(motionVector);
    outMotionColor = vec4(col, 1.0);
}
