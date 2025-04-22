#version 460 core
// #extension GL_EXT_samplerless_texture_functions : require // Not needed for texelFetch on sampler2D

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

// Corrected Set Index to 0 and Type to sampler2D
layout (set = 0, binding = 0) uniform sampler2D gbuffer_position; // Position/Depth (example)
layout (set = 0, binding = 1) uniform sampler2D gbuffer_normal;   // Normal/Roughness (example)
layout (set = 0, binding = 2) uniform sampler2D gbuffer_albedo;   // Albedo (BaseColor)
layout (set = 0, binding = 3) uniform sampler2D gbuffer_properties; // Metallic/IOR/Type (example)
layout (set = 0, binding = 4) uniform sampler2D rt_output;        // Ray Traced Reflections/Color

void main() {
    // Sample G-Buffer using texelFetch requires integer coordinates
    ivec2 texelCoord = ivec2(gl_FragCoord.xy);

    // Access sampler2D uniforms directly, remove sampler2D() constructor
    vec4 albedoColor = texelFetch(gbuffer_albedo, texelCoord, 0); // Sample albedo

    // Sample Ray Tracing Output
    vec4 rtColor = texelFetch(rt_output, texelCoord, 0); // Sample RT output

    // --- Simple Compositing Logic (Example: Additive Reflections) ---
    outColor = albedoColor + rtColor;

    // Example: Alpha blend based on RT alpha (if rtColor.a stores reflection strength)
    // outColor = vec4(mix(albedoColor.rgb, rtColor.rgb, rtColor.a), 1.0);

    // Clamp final color
    outColor = clamp(outColor, 0.0, 1.0);

    // Gamma correction (if needed)
    // outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
}