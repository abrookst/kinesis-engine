#include "rendersystem.h"
#include "mesh/material.h" // <<< Added include for Material definition >>>
#include "mesh/mesh.h"     // <<< Added include for Mesh definition >>>
#include "renderer.h" // Include renderer to access SwapChain object
#include "gbuffer.h"  // <<< Include G-Buffer header >>>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cout/cerr
#include <cassert>   // For assert

// Make sure kinesis.h is included directly or indirectly for g_Device etc.
#include "kinesis.h"

namespace Kinesis{

    // Define structure matching shader push constants (GBuffer version)
    struct GBufferPushConstantData
    {
        alignas(16) glm::mat4 modelMatrix{1.f};
        alignas(16) glm::mat4 normalMatrix{1.f}; // Often mat3 is enough, check shader
        alignas(16) glm::vec3 baseColor{1.f};    // Pad vec3 to alignas(16)
        alignas(4) float roughness{0.5f};
        alignas(4) float metallic{0.0f};
        alignas(4) float ior{1.5f};        // Default IOR
        alignas(4) int materialType{0};    // Default to Diffuse
        // Check shader definition for exact layout, ensure C++ matches std140/std430 rules if applicable
    };


    // RenderSystem Constructor
    RenderSystem::RenderSystem(){
        try {
            createPipelineLayout(); // Layout needs push constants for GBuffer shader
            // Pipeline creation depends on the G-Buffer render pass now
            // Ensure GBuffer is initialized *before* RenderSystem
            assert(Kinesis::GBuffer::renderPass != VK_NULL_HANDLE && "G-Buffer must be initialized before creating RenderSystem");
            createPipeline();
        } catch (const std::exception& e) {
            std::cerr << "RenderSystem Creation Failed: " << e.what() << std::endl;
            // Clean up partially created resources if necessary
            if (pipelineLayout != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(g_Device, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }
            throw; // Rethrow to signal failure
        }
    }

    // RenderSystem Destructor
    RenderSystem::~RenderSystem(){
         // Destroy pipeline layout
        if (pipelineLayout != VK_NULL_HANDLE) {
            // Ensure device is valid before destroying
            if (g_Device != VK_NULL_HANDLE) {
                 vkDestroyPipelineLayout(g_Device, pipelineLayout, nullptr);
            }
            pipelineLayout = VK_NULL_HANDLE;
        }
        // The VkPipeline object (graphicsPipeline in pipeline.cpp)
        // should be cleaned up globally by Pipeline::cleanup()
    }


    void RenderSystem::createPipelineLayout()
    {
         if (g_Device == VK_NULL_HANDLE) {
            throw std::runtime_error("Device not initialized before creating pipeline layout");
        }

        // Descriptor Set Layouts
        // Set 0: Global data (like Camera UBO)
        // Set 1: Per-Material data (like textures - if needed) - Add later if required

        VkDescriptorSetLayoutBinding globalUBOLayoutBinding{};
        globalUBOLayoutBinding.binding = 0; // Binding 0 in set 0
        globalUBOLayoutBinding.descriptorCount = 1;
        globalUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalUBOLayoutBinding.pImmutableSamplers = nullptr;
        globalUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible by VS & FS

        std::vector<VkDescriptorSetLayoutBinding> globalSetLayoutBindings = {globalUBOLayoutBinding};

        VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
        globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        globalLayoutInfo.bindingCount = static_cast<uint32_t>(globalSetLayoutBindings.size());
        globalLayoutInfo.pBindings = globalSetLayoutBindings.data();

        // Note: The actual globalSetLayout is now created and managed in kinesis.cpp
        // Here, we just need a layout handle that matches the one created there.
        // It's better practice to pass the layout handle from where it's created.
        // For now, we assume the layout created in RenderSystem matches the global one.
        // This is fragile. Consider refactoring layout creation.
        VkDescriptorSetLayout tempGlobalLayout = Kinesis::globalSetLayout; // Use the handle from kinesis.cpp
        if (tempGlobalLayout == VK_NULL_HANDLE) {
             throw std::runtime_error("Global descriptor set layout (Kinesis::globalSetLayout) not created before pipeline layout!");
        }


        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        // Define push constant range for GBuffer shader
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Ensure shader uses these stages
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(GBufferPushConstantData); // Use the correct struct size

        // Pass the global descriptor set layout handle
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { tempGlobalLayout };
        // Add other set layouts here if needed (e.g., texture set layout)

        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            // No need to destroy tempGlobalLayout here as it's managed elsewhere (kinesis.cpp)
            throw std::runtime_error("failed to create GBuffer pipeline layout");
        }
    }

    void RenderSystem::createPipeline()
    {
        // Ensure GBuffer is initialized before creating the pipeline
        assert(Kinesis::GBuffer::renderPass != VK_NULL_HANDLE && "GBuffer RenderPass must be initialized before creating GBuffer pipeline");
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before creating GBuffer pipeline");
         if (g_Device == VK_NULL_HANDLE) {
             throw std::runtime_error("Device not initialized before creating GBuffer pipeline");
         }

        Kinesis::Pipeline::ConfigInfo configInfo{};
        Kinesis::Pipeline::defaultConfigInfo(configInfo); // Get defaults

        // <<< Use G-Buffer Render Pass >>>
        configInfo.renderPass = Kinesis::GBuffer::renderPass;
        configInfo.pipelineLayout = pipelineLayout; // Use the layout created in this class

        // <<< Adjust Color Blend Attachments for Multiple Outputs >>>
        // We need one blend state per *color* attachment in the G-Buffer render pass
        VkPipelineColorBlendAttachmentState blendAttachmentState{}; // Common settings
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachmentState.blendEnable = VK_FALSE; // No blending for G-Buffer generation

        // Match the number of color attachments defined in GBuffer::createRenderPass
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(4, blendAttachmentState); // Position, Normal, Albedo, Properties
        configInfo.colorBlendInfo.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
        configInfo.colorBlendInfo.pAttachments = blendAttachmentStates.data();
        // Keep logicOpEnable = VK_FALSE typically for G-buffer

        // Create/Recreate the graphics pipeline using Pipeline::initialize
        // This assumes Pipeline::initialize handles destroying the *previous* pipeline if called multiple times.
        try {
             // <<< Use G-Buffer Shader Paths >>>
            // Adjust paths based on your actual build structure relative to execution directory
            #if __APPLE__
                // Define Apple paths if different
                const std::string vertPath = "../../../../../../kinesis/assets/shaders/bin/gbuffer.vert.spv";
                const std::string fragPath = "../../../../../../kinesis/assets/shaders/bin/gbuffer.frag.spv";
            #else // Assuming Windows/Linux path
                 const std::string vertPath = "../../../kinesis/assets/shaders/bin/gbuffer.vert.spv";
                 const std::string fragPath = "../../../kinesis/assets/shaders/bin/gbuffer.frag.spv";
            #endif
            Kinesis::Pipeline::initialize(vertPath, fragPath, configInfo);
            std::cout << "GBuffer Pipeline Initialized." << std::endl;
        } catch (const std::exception& e) {
             std::cerr << "GBuffer Pipeline Initialization Failed: " << e.what() << std::endl;
             // Cleanup pipeline layout if pipeline creation fails
             if (pipelineLayout != VK_NULL_HANDLE) {
                 vkDestroyPipelineLayout(g_Device, pipelineLayout, nullptr);
                 pipelineLayout = VK_NULL_HANDLE;
             }
             throw; // Re-throw
        }
    }

    // Renders game objects into the G-Buffer
    void RenderSystem::renderGameObjects(VkCommandBuffer commandBuffer, const Camera& /*camera*/, VkDescriptorSet globalDescriptorSet) {
        // Bind the G-Buffer graphics pipeline (assuming Pipeline::bind binds the latest created one)
        Kinesis::Pipeline::bind(commandBuffer);

        // Bind the global descriptor set (containing camera UBO) at set 0
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, // Use the G-Buffer pipeline layout
            0, // set number (assuming global data is at set 0)
            1, // descriptorSetCount
            &globalDescriptorSet, // The descriptor set passed in (contains camera UBO)
            0, nullptr); // No dynamic offsets

        // --- Optional: Bind texture descriptor sets if needed (e.g., at set 1) ---
        // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, ...);

        // Iterate through game objects
        for(GameObject& gObj : gameObjects){
            // Skip objects without a valid model or mesh
            if (gObj.model == nullptr || gObj.model->getMesh() == nullptr || gObj.model->getMesh()->numVertices() == 0) continue;

             // Get material - Assuming first material for simplicity.
             // A real system would handle multiple materials per mesh.
             Kinesis::Mesh::Material* mat = nullptr;
             const auto& materials = gObj.model->getMesh()->getMaterials(); // Use accessor
             if (!materials.empty()){
                 mat = materials[0]; // Use accessor
             } else {
                 // Handle missing material? Use a default? Skip?
                  std::cerr << "Warning: GameObject '" << gObj.name << "' has no materials, skipping draw." << std::endl;
                 continue; // Skip rendering if no material
             }
             if (!mat) continue; // Should not happen if materials vector wasn't empty, but check anyway

            // Prepare push constant data with transform and material info
            GBufferPushConstantData push{};
            push.modelMatrix = gObj.transform.mat4();
            // Calculate normal matrix (transpose of inverse of model matrix's upper 3x3)
            // Ensure the matrix used for inversion doesn't have scale if normals aren't scaled
            push.normalMatrix = glm::transpose(glm::inverse(glm::mat3(push.modelMatrix)));

            // --- Get material properties ---
            push.baseColor = mat->getDiffuseColor(); // Get base color from material
            push.roughness = mat->getRoughness();
            push.ior = mat->getIOR();
            push.materialType = static_cast<int>(mat->getType());
            // Determine metallic based on material type (simple example)
            push.metallic = (mat->getType() == Kinesis::Mesh::MaterialType::METAL) ? 1.0f : 0.0f;

            // Push constants to the pipeline
            vkCmdPushConstants(
                commandBuffer,
                pipelineLayout, // The layout associated with the bound pipeline
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // Stages accessing the constants
                0, // Offset
                sizeof(GBufferPushConstantData), // Size
                &push); // Pointer to the data

            // Bind the game object's model (vertex/index buffers)
            gObj.model->bind(commandBuffer);
            // Issue the draw command
            gObj.model->draw(commandBuffer);
        }
    }

} // namespace Kinesis