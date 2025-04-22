#version 460
#extension GL_EXT_ray_tracing : require

// Payload input from RayGen
layout(location = 0) rayPayloadInEXT vec3 hitValue;

// Optional: Access camera data if needed for sky color calculation
// layout(set = 0, binding = 0, std140) uniform CameraBufferObject { ... } cam;

void main() {
    // Set a background color (e.g., simple blue) when a ray misses everything
    hitValue = vec3(0.1, 0.2, 0.4);
}