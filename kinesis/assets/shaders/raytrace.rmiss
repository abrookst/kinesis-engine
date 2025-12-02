// fileName: kinesis/assets/shaders/raytrace.rmiss
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitPayload {
    vec3 hitColor;
    vec3 attenuation;
    vec3 nextRayOrigin;
    vec3 nextRayDir;
    int done;
    uint seed;
} payload;

// Shadow payload
layout(location = 1) rayPayloadInEXT bool isShadowed;

void main() {
    // Check which payload location this is
    if (isShadowed) {
        // Shadow ray missed - not occluded
        isShadowed = false;
    } else {
        // Regular ray missed - hit sky
        vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
        float t = 0.5 * (rayDir.y + 1.0);
        
        // Gradient Sky (Blue-ish to White)
        vec3 skyColor = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);

        payload.hitColor = skyColor;
        payload.attenuation = vec3(0.0);
        payload.done = 1; // Stop tracing
    }
}