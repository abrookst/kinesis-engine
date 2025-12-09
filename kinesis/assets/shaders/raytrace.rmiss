// fileName: kinesis/assets/shaders/raytrace.rmiss
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

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

// Include shared skybox function
#include "skybox.glsl"

void main() {
    // Check which payload location this is
    if (isShadowed) {
        // Shadow ray missed - not occluded
        isShadowed = false;
    } else {
        // Regular ray missed - hit sky
        vec3 rayDir = normalize(gl_WorldRayDirectionEXT);
        vec3 skyColor = getSkyColor(rayDir);

        payload.hitColor = skyColor;
        payload.attenuation = vec3(0.0);
        payload.done = 1; // Stop tracing
    }
}