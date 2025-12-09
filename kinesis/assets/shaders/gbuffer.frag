#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input from Vertex Shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragWorldNormal;
layout(location = 2) in vec3 fragColor; // Base color from vertex
layout(location = 3) in vec2 fragTexCoord;

// Output G-Buffer attachments
layout(location = 0) out vec4 outPosition;    // Attachment 0: World Position (XYZ) + ? (W)
layout(location = 1) out vec4 outNormalRough; // Attachment 1: World Normal (XYZ) + Roughness (W)
layout(location = 2) out vec4 outAlbedo;      // Attachment 2: Albedo/Base Color (RGB) + ? (A)
layout(location = 3) out vec4 outProperties;  // Attachment 3: Metallic(R), IOR(G), Type(B), ?(A)


// --- Material Information (Example: Pass via Push Constant or UBO/SSBO) ---
// This is a placeholder. Ideally, get material properties from a buffer
// based on the primitive/object being rendered.
// Using push constants is simpler for now but limited in size.
layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    mat4 normalMatrix;
    vec3 baseColor;       // Passed instead of using vertex color directly?
    float roughness;
    float metallic;        // 0 for dielectric, 1 for metal
    float ior;             // Index of Refraction (relevant for dielectrics)
    int materialType;      // 0: Diffuse, 1: Metal, 2: Dielectric
} material;
// ----------------------------------------------------------------------

// Example texture sampler (if materials use textures)
// layout(set = 1, binding = 0) uniform sampler2D texSampler;


void main() {
    // --- 1. Calculate Final Albedo ---
    // vec3 finalAlbedo = texture(texSampler, fragTexCoord).rgb * fragColor; // Example with texture * vertex color
    vec3 finalAlbedo = material.baseColor; // Using push constant base color directly for simplicity

    // --- 2. Write to G-Buffer ---

    // Attachment 0: World Position
    // Store world position. W component could be used for something else if needed.
    outPosition = vec4(fragWorldPos, 1.0);

    // Attachment 1: World Normal + Roughness
    // Ensure normal is normalized. Store roughness in the alpha channel.
    outNormalRough = vec4(normalize(fragWorldNormal), material.roughness);

    // Attachment 2: Albedo
    // Store the final base color. Alpha = 0.0 for dielectrics (transparent), 1.0 for opaque
    float alphaValue = (material.materialType == 2) ? 0.0 : 1.0; // Type 2 = DIELECTRIC
    outAlbedo = vec4(finalAlbedo, alphaValue);

    // Attachment 3: Properties
    // Pack material properties. Example packing:
    // R: Metallic factor (0.0 to 1.0)
    // G: Index of Refraction (remap if needed, e.g., (ior - 1.0) / 2.0 to fit [0,1])
    // B: Material Type ID (e.g., 0.0, 0.5, 1.0 corresponding to types 0, 1, 2)
    // A: Could be emissive mask, transparency, etc. (unused for now)
    float packedIor = (material.ior - 1.0) * 0.5; // Example remapping IOR (1.0 to 3.0 -> 0.0 to 1.0)
    float packedType = float(material.materialType) / 2.0; // Map 0,1,2 -> 0.0, 0.5, 1.0
    outProperties = vec4(material.metallic, packedIor, packedType, 1.0);

}