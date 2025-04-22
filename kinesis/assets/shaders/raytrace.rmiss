#version 460 core
#extension GL_EXT_ray_tracing : require

// Minimal Payload
struct RayPayload {
    vec3 color;
};

// Input payload
layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
    // Set fixed background color on miss
    payload.color = vec3(0.1, 0.1, 0.15); // Dark grey/blue
}