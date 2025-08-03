#version 460

// Inputs from vertex shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec4 fragKeyParams;    // midiNote, isBlack, blackAbove, blackBelow
layout(location = 3) in vec4 fragKeyTransform; // x, y, width, height

// Output
layout(location = 0) out vec4 outColor;

// Piano key shape generation function (EXACT COPY from MatrixWidget logic)
bool isInsidePianoKey(vec2 uv, vec4 keyParams) {
    float midiNote = keyParams.x;
    bool isBlack = keyParams.y > 0.5;
    bool blackOnTop = keyParams.z > 0.5;
    bool blackBeneath = keyParams.w > 0.5;

    // UV coordinates are in [0,1] range
    float x = uv.x;
    float y = uv.y;

    if (isBlack) {
        // Black keys are simple rectangles (EXACT COPY from MatrixWidget)
        return true;
    }

    // White key complex shape generation (EXACT COPY from MatrixWidget lines 889-907)
    float scaleWidthBlack = 0.6;
    float scaleHeightBlack = 0.5;

    // Generate the exact same polygon as MatrixWidget
    // The key insight: MatrixWidget creates complex polygons, we approximate with fragment discard

    if (!blackOnTop && !blackBeneath) {
        // Case: E-F and B-C transitions (no black keys adjacent)
        // Full rectangle shape
        return true;
    } else if (blackOnTop && !blackBeneath) {
        // Case: C and F keys (black key above only)
        // Cut out bottom-right corner where black key would overlap (Y is flipped in UV space)
        if (y > (1.0 - scaleHeightBlack) && x > scaleWidthBlack) {
            return false; // Cut out bottom-right corner
        }
        return true;
    } else if (!blackOnTop && blackBeneath) {
        // Case: E and B keys (black key below only)
        // Cut out top-right corner where black key would overlap (Y is flipped in UV space)
        if (y < scaleHeightBlack && x > scaleWidthBlack) {
            return false; // Cut out top-right corner
        }
        return true;
    } else {
        // Case: D, G, A keys (black keys both above and below)
        // Cut out both corners (Y coordinates are correct as-is for double notch)
        if ((y < scaleHeightBlack && x > scaleWidthBlack) ||
            (y > (1.0 - scaleHeightBlack) && x > scaleWidthBlack)) {
            return false; // Cut out both corners
        }
        return true;
    }
}

void main() {
    // Check if fragment is inside the piano key shape
    if (!isInsidePianoKey(fragUV, fragKeyParams)) {
        discard; // Don't render this fragment
    }
    
    // Use solid colors exactly like software MatrixWidget (no gradients or effects)
    // Software uses simple solid colors from Appearance class with no visual effects
    outColor = fragColor;
}
