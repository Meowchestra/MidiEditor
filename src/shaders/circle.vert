#version 460

// Vertex attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

// Instance attributes (per circle)
layout(location = 2) in vec4 circleTransform; // x, y, radius, padding
layout(location = 3) in vec4 circleColor;     // r, g, b, a

// Uniform buffer (MUST MATCH other shaders exactly)
layout(std140, binding = 0) uniform UniformBuffer {
    mat4 mvpMatrix;
    vec2 screenSize;
    float time;
    float padding;
    vec4 backgroundColor;
    vec4 stripHighlightColor;
    vec4 stripNormalColor;
    vec4 rangeLineColor;
    vec4 borderColor;
    vec4 measureLineColor;
    vec4 playbackCursorColor;
    vec4 recordingIndicatorColor;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec4 fragCircleTransform;

void main() {
    // Transform vertex position to circle space
    vec3 circlePos = position;
    circlePos.xy *= circleTransform.z * 2.0; // Scale by diameter
    circlePos.xy += circleTransform.xy;      // Translate to circle center
    
    // Convert to normalized device coordinates
    vec2 screenPos = circlePos.xy / ubo.screenSize;
    screenPos = screenPos * 2.0 - 1.0;
    screenPos.y = -screenPos.y; // Flip Y coordinate
    
    gl_Position = vec4(screenPos, 0.0, 1.0);
    
    // Pass data to fragment shader
    fragColor = circleColor;
    fragUV = uv;
    fragCircleTransform = circleTransform;
}
