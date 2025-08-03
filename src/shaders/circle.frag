#version 460

// Inputs from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec4 fragCircleTransform; // x, y, radius, padding

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Convert UV to centered coordinates [-1, 1]
    vec2 center = fragUV * 2.0 - 1.0;
    
    // Calculate distance from center
    float distance = length(center);
    
    // Discard fragments outside the circle
    if (distance > 1.0) {
        discard;
    }
    
    // Apply smooth antialiasing at the edge
    float alpha = 1.0 - smoothstep(0.9, 1.0, distance);
    
    // Apply subtle gradient for 3D effect
    float gradient = 1.0 - distance * 0.3;
    vec4 finalColor = fragColor;
    finalColor.rgb *= gradient;
    finalColor.a *= alpha;
    
    outColor = finalColor;
}
