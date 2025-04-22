#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable // Or std430 if preferred for blocks
#extension GL_GOOGLE_include_directive : enable

// --- Structures ---

// Define structure for the ray payload
struct RayPayload {
    vec3 hitValue;       // Color accumulated along the ray path
    float hitT;          // Distance to the hit point (can be updated by intersection)
    uint recursionDepth; // Current recursion depth
    // Add other needed payload members (e.g., PRNG state, flags)
};

// Define structure for hit information (passed via hitAttributeEXT)
// Ensure this matches the attributes output by your intersection shader (if any)
// or how you compute/store hit details.
struct HitInfo {
    vec3 normal;        // World-space normal at the hit point
    float hitT;         // Redundant? gl_HitTEXT provides this. Included for completeness.
    uint materialIndex; // Index into the material buffer for this geometry instance
    bool frontFacing;   // True if the ray hit the front face
    // Add other needed hit attributes (e.g., barycentric coordinates, texture coords)
};

// Define structure matching your C++ MaterialData struct
// Using std430 layout for SSBO compatibility
layout(std430) struct MaterialData {
    vec3 baseColor;
    vec3 emissiveColor;
    float roughness;
    float metallic;
    float ior;      // Index of Refraction
    int type;       // 0: Diffuse, 1: Metal, 2: Dielectric, 3: Light
    // Add padding here if necessary to match C++ std430 alignment/size
};


// --- Descriptor Set Bindings ---

// Camera Uniform Buffer Object (Set 0, Binding 0 - Matches C++ globalSetLayout)
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 projection;
    mat4 view;
    mat4 inverseProjection;
    mat4 inverseView;
} cameraData;

// Material Storage Shader Buffer Object (Set 1, Binding 2 - Matches C++ rtDescriptorSetLayout)
// Uses the MaterialData struct defined above
layout(std430, set = 1, binding = 2) buffer MaterialBuffer {
     MaterialData materials[]; // Array of materials
} materialAccess;

// Top Level Acceleration Structure (Set 1, Binding 0 - Matches C++ rtDescriptorSetLayout)
layout(set = 1, binding = 0) uniform accelerationStructureEXT topLevelAS;


// --- Shader Inputs/Outputs ---

// Incoming ray payload
layout(location = 0) rayPayloadInEXT RayPayload prdct;

// Hit attributes (populated by hardware or intersection shader)
// Note: Direct use of gl_PrimitiveID, gl_InstanceCustomIndexEXT etc. is common too
hitAttributeEXT HitInfo hitData;


// --- Helper Functions ---

// Schlick approximation for Fresnel reflectance
float fresnel_schlick(float cos_theta_i, float ior_ratio) {
    // Using eta_ratio = eta_incident / eta_transmitted
    float r0 = (1.0 - ior_ratio) / (1.0 + ior_ratio);
    r0 = r0 * r0;
    // Clamp cos_theta_i to avoid issues with pow when negative
    cos_theta_i = max(cos_theta_i, 0.0); // Ensure cos_theta is non-negative for the formula
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta_i, 5.0); // Use 1.0 - cos_theta for Schlick
}

// Function to trace a new ray
// Adapt this function signature and implementation based on your needs
// (e.g., passing PRNG state, handling different ray types)
vec3 traceRay(vec3 origin, vec3 direction, uint recursionDepth) {
    // Basic recursion depth limit
    if (recursionDepth >= 4) return vec3(0.0); // Return black or background color

    // Initialize payload for the new ray
    RayPayload newPayload;
    newPayload.recursionDepth = recursionDepth;
    newPayload.hitValue = vec3(0.0); // Initialize color contribution to black
    newPayload.hitT = 10000.0; // Initialize hit distance to far away

    // Define ray flags, masks, and SBT parameters
    uint rayFlags = gl_RayFlagsOpaqueEXT; // Use opaque unless dealing with transparency/any-hit
    uint cullMask = 0xFF;                 // Standard visibility mask
    uint sbtOffset = 0;                   // SBT record offset (can depend on material)
    uint sbtStride = 0;                   // SBT record stride (can depend on material groups)
    uint missIndex = 0;                   // Index of the miss shader in the SBT

    // Trace the ray
    traceRayEXT(topLevelAS, // Use the declared TLAS uniform
                rayFlags,
                cullMask,
                sbtOffset,
                sbtStride,
                missIndex,
                origin,
                0.001,      // tmin - small offset to avoid self-intersection
                direction,
                10000.0,    // tmax - scene bounds or large value
                0);         // Payload location (matches layout location index)

    // Return the color accumulated by the traced ray (stored in newPayload.hitValue)
    return newPayload.hitValue;
}


// --- Main Shader Logic ---

void main() {
    // --- Get Material Data ---
    // Use the instance index provided by the TLAS build
    uint materialIndex = gl_InstanceCustomIndexEXT;
    // TODO: Add bounds checking for materialIndex if necessary
    // if (materialIndex >= materialAccess.materials.length()) { handle error }
    MaterialData mat = materialAccess.materials[materialIndex];

    // --- Ray and Hit Point Setup ---
    vec3 origin = gl_WorldRayOriginEXT;       // Ray origin in world space
    vec3 direction = gl_WorldRayDirectionEXT; // Ray direction in world space
    vec3 hitPoint = origin + direction * gl_HitTEXT; // Hit point in world space

    // --- Normal Calculation ---
    // Assuming hitData.normal is the correct world-space geometric normal
    // Shading normal might differ if using normal maps
    vec3 worldNormal = normalize(hitData.normal); // Ensure normalization

    // --- Determine Facing ---
    // Check if the ray hit the front or back face of the geometry
    bool frontFace = dot(direction, worldNormal) < 0.0;
    // Ensure the normal used for calculations points outwards from the surface
    vec3 faceNormal = frontFace ? worldNormal : -worldNormal;

    // --- Material Interaction ---
    vec3 scattered_color = vec3(0.0); // Final color contribution for this hit

    // --- Dielectric Material (Glass, Water, etc.) ---
    if (mat.type == 2) {
        float refract_ior = mat.ior; // Material's index of refraction
        float eta_ratio;             // Ratio of IORs: eta_incident / eta_transmitted
        vec3 outward_normal;         // Normal pointing away from the surface into the incident medium

        if (frontFace) { // Ray entering the dielectric from outside (e.g., air)
            outward_normal = worldNormal; // Already points outward relative to surface
            eta_ratio = 1.0 / refract_ior; // Assuming outside IOR is 1.0 (air)
        } else {         // Ray exiting the dielectric into outside (e.g., air)
            outward_normal = -worldNormal; // Flip normal to point outward (back into incident medium)
            eta_ratio = refract_ior / 1.0;
        }

        // Cosine of the angle between incoming direction and outward normal
        float cos_theta_i = dot(-direction, outward_normal);

        // Check for Total Internal Reflection (TIR)
        float sin_theta_t_sq = eta_ratio * eta_ratio * (1.0 - cos_theta_i * cos_theta_i);
        bool cannot_refract = sin_theta_t_sq > 1.0;

        float reflectance; // Probability of reflection
        vec3 refracted_dir; // Direction of refracted ray (if possible)

        if (cannot_refract) {
            reflectance = 1.0; // Must reflect due to TIR
        } else {
            // Calculate reflectance using Fresnel equation (Schlick's approximation)
            reflectance = fresnel_schlick(cos_theta_i, eta_ratio);
            // Calculate refraction direction using Snell's Law
            float cos_theta_t = sqrt(max(0.0, 1.0 - sin_theta_t_sq));
            refracted_dir = normalize(eta_ratio * direction + (eta_ratio * cos_theta_i - cos_theta_t) * outward_normal);
        }

        // Calculate reflection direction (always possible)
        vec3 reflected_dir = reflect(direction, outward_normal);

        // --- Trace Reflection or Refraction ---
        // Simple probabilistic choice based on reflectance.
        // For more accuracy, could trace both and blend, or use Russian Roulette.
        if (reflectance > 0.5) { // Reflect (using 0.5 threshold, could use random float)
            scattered_color = traceRay(hitPoint + outward_normal * 0.0001, reflected_dir, prdct.recursionDepth + 1);
            // Reflection from dielectrics is typically not tinted (clear reflection)
        } else { // Refract
            scattered_color = traceRay(hitPoint - outward_normal * 0.0001, refracted_dir, prdct.recursionDepth + 1);
            // Apply filter color for transmission through colored glass/dielectric
            scattered_color *= mat.baseColor; // Tint based on material's base color
        }

    // --- Diffuse Material ---
    } else if (mat.type == 0) {
        // Example: Lambertian reflection (very basic)
        // A proper implementation would involve sampling lights or tracing a scattered ray
        scattered_color = mat.baseColor * 0.5; // Placeholder: Assume ambient light only

        // Example for tracing a scattered ray:
        // vec3 random_dir = normalize(faceNormal + random_unit_vector()); // Need a random function
        // scattered_color = mat.baseColor * traceRay(hitPoint + faceNormal * 0.0001, random_dir, prdct.recursionDepth + 1);
        // pdf = max(0.001, dot(faceNormal, random_dir) / 3.14159265);
        // scattered_color /= pdf; // Basic Monte Carlo estimator

    // --- Metal Material ---
    } else if (mat.type == 1) {
        vec3 reflected_dir = reflect(normalize(direction), faceNormal);
        // Add fuzziness for rough metals (requires random function)
        // reflected_dir = normalize(reflected_dir + mat.roughness * random_in_unit_sphere());
        scattered_color = mat.baseColor * traceRay(hitPoint + faceNormal * 0.0001, reflected_dir, prdct.recursionDepth + 1);
        // Metal reflection is tinted by its base color

    // --- Light Material ---
    } else if (mat.type == 3) {
        // Light sources simply emit their color, ending the ray path here for emission
        scattered_color = mat.emissiveColor;
    } else {
        // Unknown material type - return an error color
        scattered_color = vec3(1.0, 0.0, 1.0); // Magenta
    }

    // --- Finalize Payload ---
    // Write the calculated color contribution back to the payload
    prdct.hitValue = scattered_color;
    // Optionally update hit distance if needed elsewhere
    // prdct.hitT = gl_HitTEXT;
}