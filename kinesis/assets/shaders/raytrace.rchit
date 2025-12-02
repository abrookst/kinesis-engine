// fileName: kinesis/assets/shaders/raytrace.rchit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

// --- Payload (Matches RGen) ---
layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 hitColor;
    vec3 attenuation;
    vec3 nextRayOrigin;
    vec3 nextRayDir;
    int done;
    uint seed;
} payload;

hitAttributeEXT vec2 attribs;

// --- Bindings ---
// NOTE: Make sure Binding 2 matches your C++ struct exactly (vec4s)
struct MaterialData {
   vec4 baseColor;
   vec4 emissiveColor;
   float roughness;
   float metallic;
   float ior;
   int type;
};
layout(set = 1, binding = 2, scalar) readonly buffer MaterialBuffer { MaterialData materials[]; } materialBuffer;

struct Vertex { vec3 position; vec3 color; vec3 normal; vec2 texCoord; int _pad; };
// Arrays for bindless access - indices must be aligned in C++ descriptor update!
layout(set = 1, binding = 7, scalar) readonly buffer VertexBuffer { Vertex v[]; } vertices[];
layout(set = 1, binding = 8, scalar) readonly buffer IndexBuffer { uint i[]; } indices[];

// --- Random Float Generator [0, 1) ---
float rnd(inout uint prev) {
  prev = (prev * 1664525u + 1013904223u);
  return float(prev & 0x00FFFFFF) / float(0x01000000);
}

// --- Helper: Sample Cosine Weighted Hemisphere (Diffuse) ---
vec3 sampleHemisphere(vec3 normal, inout uint seed) {
    // Sample random angles
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    
    // Cosine-weighted hemisphere sampling
    float theta = 2.0 * 3.14159265359 * r1;  // Azimuth
    float phi = acos(sqrt(r2));              // Polar angle (cosine weighted)
    
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    
    // Create local coordinate system from normal
    vec3 u = normalize(cross(abs(normal.x) < 0.9 ? vec3(1, 0, 0) : vec3(0, 1, 0), normal));
    vec3 v = cross(normal, u);
    
    // Transform to world space
    return normalize(x * u + y * v + z * normal);
}

// --- Helper: Schlick Fresnel ---
float schlick(float cosine, float ref_idx) {
    float r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

// --- Helper: Refract using Snell's law ---
bool refractRay(vec3 v, vec3 n, float ni_over_nt, out vec3 refracted) {
    vec3 uv = normalize(v);
    float dt = dot(uv, n);
    float discriminant = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt);
    
    if (discriminant > 0.0) {
        refracted = ni_over_nt * (uv - n * dt) - n * sqrt(discriminant);
        return true;
    } else {
        return false; // Total Internal Reflection
    }
}

void main() {
    uint instanceID = gl_InstanceCustomIndexEXT;
    uint primitiveID = gl_PrimitiveID;

    // --- Geometry Fetch ---
    // Requires VK_BUFFER_USAGE_STORAGE_BUFFER_BIT in C++ creation!
    uint i0 = indices[nonuniformEXT(instanceID)].i[3 * primitiveID + 0];
    uint i1 = indices[nonuniformEXT(instanceID)].i[3 * primitiveID + 1];
    uint i2 = indices[nonuniformEXT(instanceID)].i[3 * primitiveID + 2];

    vec3 n0 = vertices[nonuniformEXT(instanceID)].v[i0].normal;
    vec3 n1 = vertices[nonuniformEXT(instanceID)].v[i1].normal;
    vec3 n2 = vertices[nonuniformEXT(instanceID)].v[i2].normal;

    // Interpolate normal
    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 localNormal = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
    vec3 worldNormal = normalize(mat3(gl_ObjectToWorldEXT) * localNormal);

    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
    
    // --- Material Fetch ---
    MaterialData mat = materialBuffer.materials[instanceID];
    uint seed = payload.seed; // Local copy of seed

    // --- Material Logic ---
    // TYPE 0: DIFFUSE
    if (mat.type == 0) {
        // Sample a random direction in the hemisphere around the normal
        vec3 diffuseDir = sampleHemisphere(worldNormal, seed);
        
        // Return material properties for next bounce
        payload.hitColor = vec3(0.0);  // No direct emission
        payload.attenuation = vec3(0.5);  // 0.5 multiplier from reference code
        payload.nextRayOrigin = hitPos + worldNormal * 0.001;
        payload.nextRayDir = diffuseDir;
        payload.done = 0; // Continue tracing
    }
    // TYPE 1: METAL
else if (mat.type == 1) {
    // Perfect reflection: r = v - 2(v·n)n
    vec3 reflected = reflect(rayDir, worldNormal);
    
    // --- ADD ROUGHNESS: Perturb the reflection direction ---
    if (mat.roughness > 0.0) {
        // Generate random numbers for perturbation
        float r1 = rnd(seed);
        float r2 = rnd(seed);
        
        // Create a random vector on a sphere
        float phi = 2.0 * 3.14159265359 * r1;
        float cosTheta = 2.0 * r2 - 1.0;
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        
        vec3 randomVec = vec3(
            sinTheta * cos(phi),
            sinTheta * sin(phi),
            cosTheta
        );
        
        // Scale random vector by roughness
        vec3 perturbation = randomVec * mat.roughness;
        
        // Add perturbation to reflection direction and normalize
        reflected = normalize(reflected + perturbation);
    }
    
    // Apply metal color tint (albedo)
    vec3 metalColor = mat.baseColor.rgb;
    
    // Only scatter if the reflected ray points away from the surface
    if (dot(reflected, worldNormal) > 0.0) {
        payload.hitColor = vec3(0.0);
        payload.attenuation = metalColor; // Tint reflection with metal color!
        payload.nextRayOrigin = hitPos + worldNormal * 0.001;
        payload.nextRayDir = reflected;
        payload.done = 0; // Continue tracing
    } else {
        // Ray scattered into the surface - absorb it
        payload.hitColor = vec3(0.0);
        payload.attenuation = vec3(0.0);
        payload.done = 1; // Stop tracing
    }
}
    // TYPE 2: GLASS / DIELECTRIC
else if (mat.type == 2) {
        vec3 outwardNormal;
        float ni_over_nt;
        vec3 attenuation = vec3(1.0); // Default no absorption

        // Check if we are hitting front (entering) or back (exiting)
        if (dot(rayDir, worldNormal) > 0.0) {
            // EXITING the glass
            outwardNormal = -worldNormal;
            ni_over_nt = mat.ior; // Glass -> Air (assuming air is 1.0)
            
            // --- FIX: BEER'S LAW (Absorption) ---
            // Light has traveled distance `gl_HitTEXT` inside the material.
            // Absorb light based on distance and material color.
            // Darker baseColor = higher density/absorbance.
            vec3 absorbance = -log(mat.baseColor.rgb + vec3(0.0001)); // Prevent log(0)
            attenuation = exp(-absorbance * gl_HitTEXT); 
        } else {
            // ENTERING the glass
            outwardNormal = worldNormal;
            ni_over_nt = 1.0 / mat.ior; // Air -> Glass
        }

        vec3 refractedDir;
        float reflectProb;
        
        // --- FIX: CHECK FOR TOTAL INTERNAL REFLECTION ---
        // Attempt to calculate refraction direction
        bool canRefract = refractRay(rayDir, outwardNormal, ni_over_nt, refractedDir);

        if (canRefract) {
            // Calculate Schlick probability only if refraction is possible
            float cosine = dot(-rayDir, outwardNormal);
            reflectProb = schlick(cosine, 1.0 / mat.ior);
        } else {
            // Total Internal Reflection: Must reflect 100%
            reflectProb = 1.0;
        }

        // Stochastic Refraction/Reflection
        if (rnd(seed) < reflectProb) {
            // Reflection
            vec3 reflected = reflect(rayDir, outwardNormal);
            payload.nextRayDir = reflected;
            payload.attenuation = attenuation; // Carry any absorption from inside
        } else {
            // Refraction
            payload.nextRayDir = refractedDir;
            payload.attenuation = attenuation; // Carry any absorption
        }

        payload.hitColor = vec3(0.0);
        payload.nextRayOrigin = hitPos + payload.nextRayDir * 0.001;
        payload.done = 0;
    }
        // TYPE 3: LIGHT / EMISSIVE

    payload.seed = seed; // Update payload seed
}