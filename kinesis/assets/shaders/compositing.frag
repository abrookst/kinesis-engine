#version 460 core
// #extension GL_EXT_samplerless_texture_functions : require // Not needed for texelFetch on sampler2D

layout (location = 0) in vec2 inUV; // Usually not needed for fullscreen triangle, but keep if used elsewhere
layout (location = 0) out vec4 outColor;

// G-Buffer Samplers (Corrected Set Index)
layout (set = 0, binding = 0) uniform sampler2D gbuffer_position;
layout (set = 0, binding = 1) uniform sampler2D gbuffer_normal;
layout (set = 0, binding = 2) uniform sampler2D gbuffer_albedo;
layout (set = 0, binding = 3) uniform sampler2D gbuffer_properties;

// Raytracing Output / Fallback Sampler (Corrected Set Index)
layout (set = 0, binding = 4) uniform sampler2D rt_output;

// Push constant block to receive the flag from C++
layout(push_constant) uniform PushData {
    int isRaytracingActive; // 1 if rt_output contains valid RT data, 0 otherwise
} pushConstants;

void main() {
    // Use gl_FragCoord for fullscreen pass
    // gl_FragCoord has origin at bottom-left with Y increasing upward
    // We need to flip Y to match texture coordinates (top-left origin)
    ivec2 texelCoord = ivec2(gl_FragCoord.xy);
    texelCoord.y = textureSize(gbuffer_albedo, 0).y - texelCoord.y - 1;  // Flip Y

    // Sample required G-Buffer components
    vec4 albedoColor = texelFetch(gbuffer_albedo, texelCoord, 0);
    // You might want to sample other G-Buffer textures (position, normal, properties)
    // here if you plan to do more complex lighting/compositing than simple addition.
    // vec4 positionSample = texelFetch(gbuffer_position, texelCoord, 0);
    // vec4 normalSample = texelFetch(gbuffer_normal, texelCoord, 0);
    // vec4 propertiesSample = texelFetch(gbuffer_properties, texelCoord, 0);

    vec4 rtContribution = vec4(0.0); // Initialize raytracing contribution to zero

    // Check the flag passed from C++ via push constants
    if (pushConstants.isRaytracingActive == 1) {
        // Only sample the rt_output texture if the flag indicates it has valid data
        rtContribution = texelFetch(rt_output, texelCoord, 0);
    }

    // --- Compositing Logic ---
    // Decide how to combine the G-Buffer data and the (potential) raytracing contribution.
    // Example 1: Simple additive blending (assumes rtContribution is emissive/reflective light)
    vec3 finalColor = albedoColor.rgb + rtContribution.rgb;

    // Example 2: Alpha blending (if rtContribution.a represents blend factor)
    // vec3 finalColor = mix(albedoColor.rgb, rtContribution.rgb, rtContribution.a);

    // Example 3: Using G-Buffer for basic lighting and adding RT reflections
    // vec3 basicLight = albedoColor.rgb * 0.5; // Simplified ambient light
    // finalColor = basicLight + rtContribution.rgb; // Additive reflections

    // Choose the compositing method that makes sense for your renderer.
    // For now, we'll stick with the simple additive blend from the original shader.
    outColor = vec4(finalColor, albedoColor.a); // Use albedo's alpha or 1.0

    // Clamp final color to valid range
    outColor = clamp(outColor, 0.0, 1.0);

    // Optional: Gamma correction if rendering to an sRGB swapchain image format
    // and the intermediate buffers are linear.
    // outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
}