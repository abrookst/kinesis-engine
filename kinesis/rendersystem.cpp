#include "rendersystem.h"
#include "renderer.h" // Include renderer to access SwapChain object
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cout/cerr
#include <cassert>   // For assert

namespace Kinesis{

    // RenderSystem Constructor
    RenderSystem::RenderSystem(){
        try {
            createPipelineLayout();
            // Pipeline creation depends on the swapchain's render pass,
            // so ensure Renderer::Initialize() was called before creating RenderSystem.
            createPipeline();
        } catch (const std::exception& e) {
            std::cerr << "RenderSystem Creation Failed: " << e.what() << std::endl;
            // Rethrow or handle appropriately
            throw;
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
        // Note: The VkPipeline object itself is cleaned up by Pipeline::cleanup()
        // which should be called during main application cleanup (e.g., in Window::cleanup)
    }


    void RenderSystem::createPipelineLayout()
    {
         if (g_Device == VK_NULL_HANDLE) {
            throw std::runtime_error("Device not initialized before creating pipeline layout");
        }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        // Define push constant range (if used)
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Stages using the push constant
        pushConstantRange.offset = 0; // Offset within the push constant block
        pushConstantRange.size = sizeof(SimplePushConstantData); // Size of the data structure

        pipelineLayoutInfo.setLayoutCount = 0; // Set to > 0 if using descriptor sets
        pipelineLayoutInfo.pSetLayouts = nullptr; // Provide descriptor set layouts if used
        pipelineLayoutInfo.pushConstantRangeCount = 1; // Set to 0 if not using push constants
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange; // Set to nullptr if not using push constants

        if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout");
        }
    }

    void RenderSystem::createPipeline()
    {
        // Ensure Renderer and SwapChain are initialized before creating the pipeline
        assert(Kinesis::Renderer::SwapChain != nullptr && "SwapChain must be initialized before creating pipeline");
        assert(pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be created before creating pipeline");
         if (g_Device == VK_NULL_HANDLE) {
             throw std::runtime_error("Device not initialized before creating pipeline");
         }

        // std::cout << "Creating pipeline..." << std::endl; // Debug message

        Kinesis::Pipeline::ConfigInfo configInfo{};
        // Populate the config info struct with default settings
        Kinesis::Pipeline::defaultConfigInfo(configInfo);

        // Set the render pass from the globally accessible SwapChain object
        configInfo.renderPass = Kinesis::Renderer::SwapChain->getRenderPass();
        // Set the pipeline layout created earlier
        configInfo.pipelineLayout = pipelineLayout;


        try {
             // Make sure the shader paths are correct relative to your build output directory
            Kinesis::Pipeline::initialize("../../kinesis/assets/shaders/bin/simple_shader.vert.spv", // Example relative path adjustment
                                          "../../kinesis/assets/shaders/bin/simple_shader.frag.spv", // Example relative path adjustment
                                          configInfo);
        } catch (const std::exception& e) {
             std::cerr << "Pipeline Initialization Failed: " << e.what() << std::endl;
             // You might want to clean up the layout if pipeline creation fails partially
             if (pipelineLayout != VK_NULL_HANDLE) {
                 vkDestroyPipelineLayout(g_Device, pipelineLayout, nullptr);
                 pipelineLayout = VK_NULL_HANDLE;
             }
             throw; // Re-throw the exception
        }


    }

    void RenderSystem::renderGameObjects(VkCommandBuffer commandBuffer){
        // Bind the graphics pipeline associated with this render system
        Kinesis::Pipeline::bind(commandBuffer);

        for(GameObject& gObj : gameObjects){
            if (gObj.model == nullptr) continue;

            gObj.transform.rotation.y = glm::mod(gObj.transform.rotation.y + 0.01f, glm::two_pi<float>());
            gObj.transform.rotation.x = glm::mod(gObj.transform.rotation.x + 0.005f, glm::two_pi<float>());

            // Prepare push constant data
            SimplePushConstantData push{};
            push.transform = gObj.transform.mat4(); // Assuming mat2() gives the 2x2 transform part
            push.color = gObj.color; // Pass color

            // Push constants to the pipeline
            vkCmdPushConstants(
                commandBuffer,
                pipelineLayout, // The layout associated with the bound pipeline
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // Stages accessing the constants
                0, // Offset
                sizeof(SimplePushConstantData), // Size
                &push); // Pointer to the data

            // Bind the game object's model (vertex buffers)
            gObj.model->bind(commandBuffer);
            // Issue the draw command
            gObj.model->draw(commandBuffer);
        }
    }

} // namespace Kinesis