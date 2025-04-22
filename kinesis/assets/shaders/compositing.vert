#version 460 core

// Simple pass-through for fullscreen triangle
// Outputs clip-space coords and UVs for sampling

layout (location = 0) out vec2 outUV;

void main() {
    // Generate vertices for a fullscreen triangle
    // Clip space coordinates (-1 to 1)
    vec2 pos = vec2( (gl_VertexIndex << 1) & 2, gl_VertexIndex & 2 );
    gl_Position = vec4( pos * 2.0 - 1.0, 0.0, 1.0 );

    // UV coordinates (0 to 1), adjusted for triangle coverage
    outUV = pos; // Simple UV mapping for fullscreen quad/triangle
}