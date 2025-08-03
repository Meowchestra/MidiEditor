#version 460

// Input from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Uniform buffer (same as vertex shader)
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

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Render MIDI events exactly like software QPainter::drawRoundedRect(x, y, w, h, 1, 1)
    // Software: p->setPen(Appearance::borderColor()); p->setBrush(eventColor); p->drawRoundedRect(..., 1, 1);

    vec2 uv = fragTexCoord; // 0.0 to 1.0 across the rectangle

    // QPainter drawRoundedRect(1, 1) means 1-pixel corner radius
    // For a typical MIDI note (100x50 pixels), 1 pixel = 1/100 = 0.01 in UV space for width
    // Use conservative estimate: 1 pixel ≈ 0.02 in UV coordinates
    float cornerRadius = 0.02;

    // Calculate distance to rounded rectangle edge (standard SDF)
    vec2 center = vec2(0.5, 0.5);
    vec2 halfSize = vec2(0.5 - cornerRadius, 0.5 - cornerRadius);
    vec2 d = abs(uv - center) - halfSize;
    float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - cornerRadius;

    // QPainter renders: fill interior with brush color, then border with pen color
    // Software: p->setPen(Appearance::borderColor()); p->setBrush(eventColor); p->drawRoundedRect(...);

    // Fill (interior with event color)
    float fillAlpha = 1.0 - smoothstep(-0.001, 0.001, dist);

    // Border (outline with border color) - QPainter draws ~1 pixel border
    float borderWidth = 0.015; // ~1 pixel border in UV space
    float outerEdge = 1.0 - smoothstep(-0.001, 0.001, dist);
    float innerEdge = 1.0 - smoothstep(-borderWidth, -borderWidth + 0.002, dist);
    float borderAlpha = outerEdge - innerEdge;

    // Colors: event color for fill, darker color for border (matches software appearance)
    vec3 fillColor = fragColor.rgb;
    vec3 borderColor = ubo.borderColor.rgb; // Use actual border color from uniform buffer

    // Composite: fill + border (border on top like QPainter)
    vec3 finalColor = mix(fillColor, borderColor, borderAlpha / max(fillAlpha, 0.001));
    float finalAlpha = max(fillAlpha, borderAlpha);

    // DEBUG: Force bright magenta to test if fragment shader executes
    outColor = vec4(1.0, 0.0, 1.0, 1.0); // Bright magenta

    // TODO: Restore this line once we confirm shader execution:
    // outColor = vec4(finalColor, fragColor.a * finalAlpha);
}
