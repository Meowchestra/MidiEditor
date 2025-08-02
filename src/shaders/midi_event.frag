#version 460

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render MIDI events exactly like software QPainter::drawRoundedRect(x, y, w, h, 1, 1)
    // The software renderer uses radius 1,1 which is very subtle rounding
    
    vec2 pos = fragTexCoord; // 0.0 to 1.0 across the rectangle
    
    // Calculate distance from edges for rounded corners
    // QPainter radius 1,1 means 1 pixel radius, so very small
    float cornerRadius = 0.05; // Small radius to match QPainter(1,1)
    
    // Calculate distance from each corner
    vec2 corner = min(pos, 1.0 - pos); // Distance from nearest edge
    float minDist = min(corner.x, corner.y);
    
    // Create rounded corners only at the very edges
    float alpha = 1.0;
    if (minDist < cornerRadius) {
        // We're near a corner, calculate rounded corner alpha
        vec2 cornerPos = vec2(
            pos.x < 0.5 ? cornerRadius : 1.0 - cornerRadius,
            pos.y < 0.5 ? cornerRadius : 1.0 - cornerRadius
        );
        float dist = distance(pos, cornerPos);
        alpha = 1.0 - smoothstep(cornerRadius - 0.01, cornerRadius, dist);
    }
    
    // Apply the event color with rounded corner alpha
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    
    // Add subtle gradient to match software rendering appearance
    // Software rendering often has slight variations due to anti-aliasing
    float gradient = 1.0 - fragTexCoord.y * 0.1; // Very subtle gradient
    outColor.rgb *= gradient;
}
