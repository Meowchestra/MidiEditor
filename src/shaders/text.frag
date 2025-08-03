#version 460

// Inputs from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragAtlasUV;

// Font atlas texture
layout(binding = 1) uniform sampler2D fontAtlas;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Sample the font atlas texture
    vec4 atlasColor = texture(fontAtlas, fragAtlasUV);
    
    // Use the alpha channel from the atlas as the text mask
    float textAlpha = atlasColor.a;
    
    // Apply text color with atlas alpha
    vec4 finalColor = fragColor;
    finalColor.a *= textAlpha;
    
    // Discard fully transparent fragments
    if (finalColor.a < 0.01) {
        discard;
    }
    
    outColor = finalColor;
}
