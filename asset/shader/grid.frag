#version 460 core

in VS_OUT {
    vec3 worldPos;
} fs_in;

uniform float uGridSize;
uniform vec3 uCameraPos;
uniform float uFadeDistance; // Distance at which grid becomes invisible

out vec4 oPixelColor;

void main() {
    // Grid pattern using modulo
    vec3 pos = fs_in.worldPos;
    vec3 grid = abs(mod(pos, uGridSize)) - uGridSize * 0.5;
    float gridLine = length(grid.xz - clamp(grid.xz, -0.05, 0.05));
    
    // Create grid lines
    float alpha = step(gridLine, 0.1) ? 0.7 : 0.0;
    
    // Calculate distance-based fade-out (like Blender)
    float distance = length(uCameraPos - pos);
    float fadeStart = uFadeDistance * 0.5;
    float fadeEnd = uFadeDistance;
    
    // Smooth fade based on distance
    float fadeFactor = smoothstep(fadeEnd, fadeStart, distance);
    alpha *= fadeFactor;
    
    // Grid color (light gray/blue)
    vec3 gridColor = mix(
        vec3(0.3, 0.3, 0.35),  // Closer (bluer)
        vec3(0.5, 0.5, 0.5),   // Farther (grayer)
        1.0 - fadeFactor
    );
    
    // Draw grid lines
    if (alpha > 0.01) {
        oPixelColor = vec4(gridColor, alpha);
    } else {
        discard;
    }
}
