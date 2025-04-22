#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
// *** REMOVED: Unsupported extension ***
// #extension GL_EXT_ray_geometry_culling : require

// Payload input/output
layout(location = 0) rayPayloadInEXT vec3 hitValue;

// Built-in variables providing hit information
// gl_InstanceCustomIndexEXT
// gl_WorldRayOriginEXT
// gl_WorldRayDirectionEXT
// gl_HitTEXT

// --- Set 0: Global Data ---
layout(set = 0, binding = 0, std140) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseProjection;
    mat4 inverseView;
} cam;

// --- Set 1: Ray Tracing Specific Data ---
// Material Data Structure (matches C++ MaterialData)
struct MaterialData {
   vec3 baseColor;
   vec3 emissiveColor;
   float roughness;
   float metallic;
   float ior;
   int type;
};

// Binding 2: Material Buffer (read-only)
layout(set = 1, binding = 2, scalar) readonly buffer MaterialBuffer {
   MaterialData materials[];
} materialBuffer;


// --- Helper function to calculate basic Lambertian shading ---
vec3 calculateLighting(vec3 hitPos, vec3 normal, vec3 baseColor) {
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.5)); // Example fixed light direction
    float dotNL = max(0.0, dot(normal, lightDir));
    return baseColor * dotNL * 1.2 + baseColor * 0.1; // Simple diffuse + ambient term
}

void main() {
    // Get the index of the hit instance
    uint instanceIndex = gl_InstanceCustomIndexEXT;

    // Fetch the material data for this instance
    MaterialData mat = materialBuffer.materials[instanceIndex];

    // Calculate hit position in world space
    vec3 origin    = gl_WorldRayOriginEXT;
    vec3 direction = gl_WorldRayDirectionEXT;
    float hitT     = gl_HitTEXT;
    vec3 worldHitPos = origin + direction * hitT;

    // --- Get the Normal ---
    // *** FIXED: Use placeholder normal as GL_EXT_ray_geometry_culling is not supported ***
    // This normal is GEOMETRICALLY INCORRECT and only serves to let the shader compile.
    // Proper normal calculation requires passing vertex data or using supported extensions.
    vec3 normal = normalize(-direction); // Placeholder!

    // --- Basic Shading based on Material Type ---
    vec3 finalColor = vec3(0.0);
    int matType = mat.type; // Matches MaterialType enum

    // Simple Lambertian shading for Diffuse/Metal for now
    if (matType == 0 || matType == 1) { // DIFFUSE or METAL
        finalColor = calculateLighting(worldHitPos, normal, mat.baseColor);
    }
    // Add simple handling for Dielectric (just tint) or Light
    else if (matType == 2) { // DIELECTRIC
         finalColor = mat.baseColor * 0.8;
    } else if (matType == 3) { // LIGHT
        finalColor = mat.emissiveColor;
    } else {
        finalColor = vec3(1.0, 0.0, 1.0); // Magenta for unknown type
    }


    // --- Write result to payload ---
    hitValue = finalColor;
}