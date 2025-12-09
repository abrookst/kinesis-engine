#version 460 core
#extension GL_GOOGLE_include_directive : require
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
    int gbufferDebugMode;   // 0=Off, 1=Position, 2=Normal, 3=Albedo, 4=Properties
} pushConstants;

// Include shared skybox function
#include "skybox.glsl"

void main() {
    // Use gl_FragCoord for fullscreen pass
    // gl_FragCoord has origin at bottom-left with Y increasing upward
    // We need to flip Y to match texture coordinates (top-left origin)
    ivec2 texelCoord = ivec2(gl_FragCoord.xy);
    texelCoord.y = textureSize(gbuffer_albedo, 0).y - texelCoord.y - 1;  // Flip Y

    // Sample required G-Buffer components
    vec4 positionSample = texelFetch(gbuffer_position, texelCoord, 0);
    vec4 albedoColor = texelFetch(gbuffer_albedo, texelCoord, 0);
    vec4 normalRoughSample = texelFetch(gbuffer_normal, texelCoord, 0);
    vec4 propertiesSample = texelFetch(gbuffer_properties, texelCoord, 0);
    
    // G-Buffer Debug Visualization
    if (pushConstants.gbufferDebugMode > 0) {
        if (pushConstants.gbufferDebugMode == 1) {
            // Position (visualize as color)
            outColor = vec4(positionSample.xyz * 0.1, 1.0); // Scale for visibility
        } else if (pushConstants.gbufferDebugMode == 2) {
            // Normal (map -1..1 to 0..1)
            outColor = vec4(normalRoughSample.xyz * 0.5 + 0.5, 1.0);
        } else if (pushConstants.gbufferDebugMode == 3) {
            // Albedo
            outColor = vec4(albedoColor.rgb, 1.0);
        } else if (pushConstants.gbufferDebugMode == 4) {
            // Properties (visualize metallic, roughness, type)
            outColor = vec4(propertiesSample.rgb, 1.0);
        }
        return;
    }
    
    // Check if this is a background pixel (no geometry hit in G-Buffer)
    if (positionSample.w < 0.5) {
        // Background pixel - render skybox
        // Simple approximation: use screen Y position as sky direction
        vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(gbuffer_albedo, 0));
        // Create an approximate ray direction (looking forward with Y gradient)
        vec3 approxRayDir = normalize(vec3(screenUV.x * 2.0 - 1.0, screenUV.y * 2.0 - 1.0, -1.0));
        
        outColor = vec4(getSkyColor(approxRayDir), 1.0);
        return;
    }

    // Unpack G-Buffer data
    vec3 normal = normalRoughSample.xyz;
    // Check if normal is valid (not zero)
    float normalLen = length(normal);
    if (normalLen > 0.01) {
        normal = normalize(normal);
    } else {
        // Invalid normal - might be a problem with G-Buffer
        normal = vec3(0.0, 1.0, 0.0); // Default up
    }
    
    float roughness = normalRoughSample.w;
    float metallic = propertiesSample.r;
    float packedType = propertiesSample.b;
    int materialType = int(packedType * 2.0 + 0.5); // Unpack type (0.0, 0.5, 1.0 -> 0, 1, 2)
    
    vec4 rtContribution = vec4(0.0); // Initialize raytracing contribution to zero

    // Check the flag passed from C++ via push constants
    if (pushConstants.isRaytracingActive == 1) {
        rtContribution = texelFetch(rt_output, texelCoord, 0);
    }

    // --- Compositing Logic ---
    vec3 finalColor;
    
    // Check material type
    bool isDielectric = (materialType == 2); // Type 2 = DIELECTRIC
    bool isMetal = (materialType == 1); // Type 1 = METAL
    bool isDiffuse = (materialType == 0); // Type 0 = DIFFUSE
    
    // DEBUG: Visualize material types (uncomment to debug)
    // if (isDielectric) { outColor = vec4(0.0, 0.0, 1.0, 1.0); return; } // Blue = Dielectric
    // if (isMetal) { outColor = vec4(1.0, 0.0, 0.0, 1.0); return; } // Red = Metal
    // if (isDiffuse) { outColor = vec4(0.0, 1.0, 0.0, 1.0); return; } // Green = Diffuse
    
    // Always compute base rasterized lighting first (except for dielectrics which are transparent)
    vec3 rasterColor = vec3(0.0);
    
    if (!isDielectric) {
        // Apply simple Blinn-Phong shading for opaque materials
        vec3 worldPos = positionSample.xyz;
        
        // Light parameters (directional light)
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        vec3 lightColor = vec3(1.0, 0.98, 0.95) * 1.5;
        
        // View direction (camera at origin approximation)
        vec3 viewDir = normalize(-worldPos);
        
        // Ambient term
        vec3 ambient = albedoColor.rgb * 0.15;
        
        // Diffuse term
        float NdotL = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = albedoColor.rgb * NdotL * lightColor;
        
        // Specular term (Blinn-Phong)
        vec3 specular = vec3(0.0);
        if (NdotL > 0.0) {
            vec3 halfDir = normalize(lightDir + viewDir);
            float NdotH = max(dot(normal, halfDir), 0.0);
            
            if (isMetal) {
                // Metals have colored specular highlights
                float specPower = mix(32.0, 256.0, 1.0 - roughness);
                float specStrength = pow(NdotH, specPower);
                specular = albedoColor.rgb * specStrength * lightColor;
            } else {
                // Diffuse materials have white specular
                float specPower = mix(8.0, 64.0, 1.0 - roughness);
                float specStrength = pow(NdotH, specPower) * (1.0 - roughness) * 0.3;
                specular = vec3(specStrength) * lightColor;
            }
        }
        
        rasterColor = ambient + diffuse + specular;
    }
    
    // Now blend with raytracing contribution if active
    if (isDielectric && pushConstants.isRaytracingActive == 1) {
        // For dielectrics with raytracing: use ONLY the raytraced result (full refraction/reflection)
        finalColor = rtContribution.rgb;
    } else if (isMetal && pushConstants.isRaytracingActive == 1) {
        // For metals with RT: blend rasterized base with RT reflections based on roughness
        float reflectionStrength = mix(0.95, 0.3, roughness); // Smooth=95% reflection, rough=30%
        finalColor = mix(rasterColor, rtContribution.rgb, reflectionStrength);
    } else if (isDiffuse && pushConstants.isRaytracingActive == 1) {
        // For diffuse materials with RT: add RT contribution (indirect lighting/reflections)
        finalColor = rasterColor + rtContribution.rgb;
    } else {
        // No raytracing - use rasterized lighting only
        if (isDielectric) {
            // Dielectrics without RT: show dimmed
            finalColor = albedoColor.rgb * 0.3;
        } else {
            // Use the rasterized color we computed above
            finalColor = rasterColor;
        }
    }

    outColor = vec4(finalColor, 1.0);

    outColor = clamp(outColor, 0.0, 1.0);

}