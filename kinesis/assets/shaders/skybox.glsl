// skybox.glsl - Shared skybox/sky color function
// Include this in both rasterization and raytracing shaders for consistent sky appearance

vec3 getSkyColor(vec3 rayDirection) {
    vec3 dir = normalize(rayDirection);
    
    // Simple gradient from horizon to zenith
    float t = 0.5 * (dir.y + 1.0);
    
    // Gradient: Horizon (light blue/white) to Zenith (deeper blue)
    vec3 horizonColor = vec3(0.9, 0.95, 1.0);  // Light blue-white at horizon
    vec3 zenithColor = vec3(0.5, 0.7, 1.0);    // Sky blue at top
    
    return mix(horizonColor, zenithColor, t);
}
