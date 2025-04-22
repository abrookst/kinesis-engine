#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor; // Keep color if needed, maybe pass to frag
layout(location = 2) in vec3 inNormal; // Assuming normal is attribute 2
layout(location = 3) in vec2 inTexCoord; // Assuming texcoord is attribute 3

// Push constants for object transform
layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    mat4 normalMatrix; // Usually transpose(inverse(modelMatrix)) for normals
    // Add other material props needed in VS if any
} pushConstants;

// Uniform Buffer for camera
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    vec3 cameraPos; // World space camera position
} ubo;


// Output to Fragment Shader (G-Buffer data)
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragWorldNormal;
layout(location = 2) out vec3 fragColor; // Pass vertex color through
layout(location = 3) out vec2 fragTexCoord;


void main() {
    vec4 worldPos = pushConstants.modelMatrix * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz / worldPos.w; // Perspective divide for world pos

    // Transform normal to world space
    fragWorldNormal = normalize(mat3(pushConstants.normalMatrix) * inNormal);

    fragColor = inColor; // Pass color
    fragTexCoord = inTexCoord; // Pass texcoord

    gl_Position = ubo.projection * ubo.view * worldPos;
}