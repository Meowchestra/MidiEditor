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
    
    // Apply smooth antialiasing at the edge (matches software drawEllipse)
    float alpha = 1.0 - smoothstep(0.9, 1.0, distance);

    // Use solid colors exactly like software drawEllipse (no gradients or 3D effects)
    vec4 finalColor = fragColor;
    finalColor.a *= alpha;

    outColor = finalColor;
}
