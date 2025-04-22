#include <iostream>  // For std::cerr
#include <stdexcept> // For std::exception
#include <vector>    // For std::vector
#include <memory>    // For std::unique_ptr, std::make_unique, std::shared_ptr
#include <cassert>   // For assert
#include <chrono>
#include "kinesis.h"
#include "buffer.h"                     // Include the new Buffer helper
#include "gbuffer.h"                    // Include for GBuffer access
#include "raytracer/raytracermanager.h" // Include for RayTracerManager access
#include "mesh/material.h"              // Include for Kinesis::Mesh::Material

// Define structure matching shader uniform buffer object (UBO)
// Ensure layout matches shader definition (std140 alignment often used)
struct CameraBufferObject
{
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 inverseProjection; // Optional: For reconstructing world pos from depth
    alignas(16) glm::mat4 inverseView;       // Optional: For world space calculations
    // Add other global uniforms like camera position if needed
};

// Add this struct definition somewhere accessible, e.g., material.h or a new common_types.h
// Make sure layout matches GLSL std430 rules for SSBOs
struct MaterialData
{
    alignas(16) glm::vec3 baseColor;
    alignas(16) glm::vec3 emissiveColor; // Example: if materials can emit light
    alignas(4) float roughness;
    alignas(4) float metallic;
    alignas(4) float ior; // Index of Refraction
    alignas(4) int type;  // Matches MaterialType enum
    // Add padding if needed to meet std430 alignment for the whole struct
    // alignas(4) int _padding[x];
};

// <<< ADDED: Define structure matching the push constant block in the compositing shader >>>
struct CompositePushConstant {
    alignas(4) int isRaytracingActive; // Matches the 'int isRaytracingActive' in GLSL
};


#include "window.h"
#include "GUI.h"
#include "renderer.h"
#include "rendersystem.h" // Include RenderSystem header
#include "camera.h"
#include "keyboard_controller.h"

#include <iostream>  // For std::cerr
#include <stdexcept> // For std::exception
#include <vector>    // For std::vector
#include <memory>    // For std::unique_ptr, std::make_unique, std::shared_ptr
#include <cassert>   // For assert
#include <chrono>
#include <array> // For std::array

using namespace Kinesis;

namespace Kinesis
{
    // Global Vulkan Objects (defined in window.cpp or elsewhere)
    VkAllocationCallbacks *g_Allocator = nullptr;
    VkInstance g_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice g_Device = VK_NULL_HANDLE;
    uint32_t g_QueueFamily = (uint32_t)-1;
    VkQueue g_Queue = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
    VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

    ImGui_ImplVulkanH_Window g_MainWindowData;
    // Application specific objects
    RenderSystem *mainRenderSystem = nullptr;
    Camera mainCamera = Camera();
    GameObject player = GameObject::createGameObject("player");

    std::vector<GameObject> gameObjects = std::vector<GameObject>();

    // --- Additions for Global Descriptor Set ---
    std::vector<std::unique_ptr<Buffer>> uboBuffers;
    VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> globalDescriptorSets;
    // --- End Additions ---

    // --- Add Global Variable for Material Buffer ---
    std::unique_ptr<Buffer> materialBuffer = nullptr;
    std::vector<MaterialData> sceneMaterialData; // Host-side copy
    // --- End Addition ---

    // --- Add Global Variables for Compositing ---
    // Compositing Pass Resources
    VkPipelineLayout compositePipelineLayout = VK_NULL_HANDLE;
    VkPipeline compositePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout compositeSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> compositeDescriptorSets;
    // --- End Additions ---

    auto currentTime = std::chrono::high_resolution_clock::now();

    void initialize(int width, int height)
    {
        try
        {
            Kinesis::Window::initialize(width, height); // Initializes Vulkan core, window, ImGui
            // Note: Renderer::Initialize calls recreateSwapChain, which finds depth format.
            // We need the depth format for GBuffer setup.
            // Either pass depth format here or ensure SwapChain is created first.
            // Let's assume SwapChain is created inside Window::initialize->Renderer::Initialize
            assert(Kinesis::Renderer::SwapChain != nullptr && "Swapchain must be initialized before GBuffer setup!");
            VkFormat depthFormat = Kinesis::Renderer::SwapChain->findDepthFormat();
            Kinesis::GBuffer::setup(width, height, depthFormat); // Initialize GBuffer
            loadGameObjects();                                   // Loads models into gameObjects vector

            // --- Create Global UBO Buffers & Descriptor Set Layout/Sets ---
            // One UBO buffer per frame in flight
            uboBuffers.resize(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            for (int i = 0; i < uboBuffers.size(); ++i)
            {
                uboBuffers[i] = std::make_unique<Buffer>(
                    sizeof(CameraBufferObject),                                                // Use the defined struct size
                    1,                                                                         // Only one instance of the global UBO
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,                                        // Usage is UBO
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // Host visible/coherent for easy CPU updates
                );
                uboBuffers[i]->map(); // Map the buffer persistently
            }

            // Create Descriptor Set Layout (defines the structure of the set)
            VkDescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0;         // Corresponds to layout(set=0, binding=0) in shaders
            uboLayoutBinding.descriptorCount = 1; // One UBO
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.pImmutableSamplers = nullptr;
            // Make the UBO accessible to relevant shader stages
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &uboLayoutBinding;

            if (vkCreateDescriptorSetLayout(g_Device, &layoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create global descriptor set layout!");
            }

            // Allocate Descriptor Sets (one per frame in flight) from the global pool
            globalDescriptorSets.resize(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            std::vector<VkDescriptorSetLayout> layouts(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT, globalSetLayout); // Need layout array for allocation
            VkDescriptorSetAllocateInfo allocInfoSet{};                                                            // Renamed variable
            allocInfoSet.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfoSet.descriptorPool = g_DescriptorPool; // Use the global pool created in window.cpp
            allocInfoSet.descriptorSetCount = static_cast<uint32_t>(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            allocInfoSet.pSetLayouts = layouts.data();

            if (vkAllocateDescriptorSets(g_Device, &allocInfoSet, globalDescriptorSets.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate global descriptor sets!");
            }

            // Update Descriptor Sets to point them to the corresponding UBO buffers
            for (int i = 0; i < globalDescriptorSets.size(); i++)
            {
                auto bufferInfo = uboBuffers[i]->descriptorInfo(); // Get VkDescriptorBufferInfo for the buffer
                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = globalDescriptorSets[i]; // Target the specific set for this frame index
                descriptorWrite.dstBinding = 0;                   // Matches the layout binding
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo; // Point to the buffer info

                vkUpdateDescriptorSets(g_Device, 1, &descriptorWrite, 0, nullptr); // Update the set
            }
            // --- End Descriptor Set Setup ---

            // --- Create Material Buffer ---
            {
                sceneMaterialData.clear(); // Clear any previous data
                uint32_t materialIndexCounter = 0; // Assign explicit indices

                // Iterate through loaded game objects and collect material data
                for (auto &go : gameObjects) // Iterate by reference if modifying object
                {
                    if (go.model && go.model->getMesh())
                    {
                        const auto &meshMaterials = go.model->getMesh()->getMaterials();
                        if (!meshMaterials.empty())
                        {
                            // Simple case: Assume one material per mesh/object
                            // TODO: Handle multiple materials per mesh if your engine supports it.
                            // This would require the TLAS instance to somehow specify which
                            // material index within the mesh to use, or store material indices per-primitive.
                            Kinesis::Mesh::Material *mat = meshMaterials[0];
                            if (mat) { // Check if material pointer is valid
                                MaterialData data{};
                                data.baseColor = mat->getDiffuseColor();
                                data.emissiveColor = mat->getEmittedColor();
                                data.roughness = mat->getRoughness();
                                data.metallic = (mat->getType() == Kinesis::Mesh::MaterialType::METAL) ? 1.0f : 0.0f;
                                data.ior = mat->getIOR(); // <<< COPY IOR >>>
                                data.type = static_cast<int>(mat->getType()); // <<< COPY TYPE >>>
                                sceneMaterialData.push_back(data);

                                // Assign this index to the game object for potential later use
                                // (e.g., if shaders need object ID -> material ID mapping)
                                // go.materialIndex = materialIndexCounter; // Requires adding materialIndex to GameObject

                                materialIndexCounter++;
                            } else {
                                std::cerr << "Warning: GameObject '" << go.name << "' has a null material pointer in its mesh. Using default." << std::endl;
                                sceneMaterialData.push_back({}); // Push default-constructed data
                                materialIndexCounter++;
                            }
                        }
                        else
                        {
                            std::cerr << "Warning: GameObject '" << go.name << "' has no materials. Using default." << std::endl;
                            sceneMaterialData.push_back({}); // Push default-constructed data
                            materialIndexCounter++;
                        }
                    }
                    else
                    {
                         std::cerr << "Warning: GameObject '" << go.name << "' has no model. Using default material data." << std::endl;
                        sceneMaterialData.push_back({});
                        materialIndexCounter++;
                    }
                }

                // Ensure we have at least one material entry if no objects loaded
                if (sceneMaterialData.empty())
                {
                    std::cout << "Warning: No game objects with materials found. Creating one default material entry." << std::endl;
                    sceneMaterialData.push_back({}); // Add one default material
                }

                // Create the GPU buffer for materials
                materialBuffer = std::make_unique<Buffer>(
                    sizeof(MaterialData),                            // Size of one material struct
                    static_cast<uint32_t>(sceneMaterialData.size()), // Number of materials
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, // <<< ADD SHADER_DEVICE_ADDRESS for RT access >>>
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT // Optional: For better performance, use staging buffer
                );
                materialBuffer->map();
                materialBuffer->writeToBuffer(sceneMaterialData.data());
                // materialBuffer->unmap(); // Not needed if coherent
                // materialBuffer->flush(); // Needed if not coherent

                std::cout << "Material SSBO created/updated with " << sceneMaterialData.size() << " entries." << std::endl;
                std::cout << "  Size of MaterialData: " << sizeof(MaterialData) << " bytes" << std::endl;
                std::cout << "  Total Buffer Size: " << materialBuffer->getBufferSize() << " bytes" << std::endl;

            }
            // --- End Material Buffer Creation ---

            // --- Initialize Ray Tracing (if available) ---
            if (Kinesis::GUI::raytracing_available)
            {
                // Pass material buffer info to RT manager if needed for descriptors immediately
                Kinesis::RayTracerManager::initialize({static_cast<uint32_t>(width), static_cast<uint32_t>(height)});
            }

            // --- Create RenderSystem (for G-Buffer Pass) ---
            mainRenderSystem = new RenderSystem(); // Uses globalSetLayout

            // ===================================
            // --- Create Compositing Pipeline ---
            // ===================================
            {
                // --- 1. Create Compositing Descriptor Set Layout ---
                std::vector<VkDescriptorSetLayoutBinding> compositeBindings;
                // Binding 0: GBuffer Position
                compositeBindings.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
                // Binding 1: GBuffer Normal
                compositeBindings.push_back({1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
                // Binding 2: GBuffer Albedo
                compositeBindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
                // Binding 3: GBuffer Properties
                compositeBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
                // Binding 4: RT Output Image / Fallback
                compositeBindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

                VkDescriptorSetLayoutCreateInfo compositeLayoutInfo{}; // Renamed variable
                compositeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                compositeLayoutInfo.bindingCount = static_cast<uint32_t>(compositeBindings.size());
                compositeLayoutInfo.pBindings = compositeBindings.data();

                if (vkCreateDescriptorSetLayout(g_Device, &compositeLayoutInfo, nullptr, &compositeSetLayout) != VK_SUCCESS)
                {
                    throw std::runtime_error("Failed to create compositing descriptor set layout!");
                }

                // --- 2. Create Compositing Pipeline Layout ---
                // <<< MODIFIED: Add Push Constant Range >>>
                VkPushConstantRange compositePushConstantRange{};
                compositePushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible only in fragment shader
                compositePushConstantRange.offset = 0;
                compositePushConstantRange.size = sizeof(CompositePushConstant); // Use the C++ struct size

                VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
                pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                // Bind the single composite set layout (containing GBuffer/RT textures) to Set 0
                pipelineLayoutInfo.setLayoutCount = 1;
                pipelineLayoutInfo.pSetLayouts = &compositeSetLayout;
                // Add the push constant range
                pipelineLayoutInfo.pushConstantRangeCount = 1; // <<< Set count to 1 >>>
                pipelineLayoutInfo.pPushConstantRanges = &compositePushConstantRange; // <<< Point to the range >>>


                if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, nullptr, &compositePipelineLayout) != VK_SUCCESS)
                {
                    vkDestroyDescriptorSetLayout(g_Device, compositeSetLayout, nullptr); // Cleanup
                    throw std::runtime_error("Failed to create compositing pipeline layout!");
                }

                // --- 3. Create Compositing Pipeline ---
                Pipeline::ConfigInfo configInfo{};
                Pipeline::defaultConfigInfo(configInfo);                               // Get defaults
                configInfo.renderPass = Kinesis::Renderer::SwapChain->getRenderPass(); // Renders to swapchain
                configInfo.pipelineLayout = compositePipelineLayout; // Use the layout created above (now includes push constants)
                // Disable depth test/write for fullscreen quad
                configInfo.depthStencilInfo.depthTestEnable = VK_FALSE;
                configInfo.depthStencilInfo.depthWriteEnable = VK_FALSE;

                // Use composite shader paths (adjust if needed)
#if __APPLE__
                const std::string vertPath = "../../../../../../kinesis/assets/shaders/bin/compositing.vert.spv";
                const std::string fragPath = "../../../../../../kinesis/assets/shaders/bin/compositing.frag.spv"; // Assumes shader is recompiled
#else
                const std::string vertPath = "../../../kinesis/assets/shaders/bin/compositing.vert.spv";
                const std::string fragPath = "../../../kinesis/assets/shaders/bin/compositing.frag.spv"; // Assumes shader is recompiled
#endif

                // --- Load Shaders ---
                auto compositeVertCode = Pipeline::readFile(vertPath);
                auto compositeFragCode = Pipeline::readFile(fragPath);
                VkShaderModule compositeVertModule, compositeFragModule;

                auto createShaderModule = [&](const std::vector<char> &code, VkShaderModule *shaderModule)
                {
                    VkShaderModuleCreateInfo createInfo{};
                    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                    createInfo.codeSize = code.size();
                    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
                    if (vkCreateShaderModule(g_Device, &createInfo, nullptr, shaderModule) != VK_SUCCESS)
                    {
                        throw std::runtime_error("failed to create shader module!");
                    }
                };
                createShaderModule(compositeVertCode, &compositeVertModule);
                createShaderModule(compositeFragCode, &compositeFragModule);

                // --- Pipeline Create Info ---
                VkPipelineShaderStageCreateInfo shaderStages[2];
                shaderStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, compositeVertModule, "main", nullptr };
                shaderStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, compositeFragModule, "main", nullptr };


                // No vertex input for fullscreen triangle generated in VS
                VkPipelineVertexInputStateCreateInfo emptyInputState{};
                emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                emptyInputState.vertexBindingDescriptionCount = 0;
                emptyInputState.pVertexBindingDescriptions = nullptr;
                emptyInputState.vertexAttributeDescriptionCount = 0;
                emptyInputState.pVertexAttributeDescriptions = nullptr;

                VkGraphicsPipelineCreateInfo compositePipelineInfo{}; // Renamed variable
                compositePipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                compositePipelineInfo.stageCount = 2;
                compositePipelineInfo.pStages = shaderStages;
                compositePipelineInfo.pVertexInputState = &emptyInputState; // Use empty input state
                compositePipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
                compositePipelineInfo.pViewportState = &configInfo.viewportInfo;
                compositePipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
                compositePipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
                compositePipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
                compositePipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
                compositePipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;
                compositePipelineInfo.layout = compositePipelineLayout;
                compositePipelineInfo.renderPass = configInfo.renderPass; // Swapchain render pass
                compositePipelineInfo.subpass = configInfo.subpass;       // Subpass 0
                compositePipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
                compositePipelineInfo.basePipelineIndex = -1;

                if (vkCreateGraphicsPipelines(g_Device, VK_NULL_HANDLE, 1, &compositePipelineInfo, nullptr, &compositePipeline) != VK_SUCCESS)
                {
                    vkDestroyShaderModule(g_Device, compositeVertModule, nullptr);
                    vkDestroyShaderModule(g_Device, compositeFragModule, nullptr);
                    vkDestroyPipelineLayout(g_Device, compositePipelineLayout, nullptr);
                    vkDestroyDescriptorSetLayout(g_Device, compositeSetLayout, nullptr);
                    throw std::runtime_error("Failed to create compositing graphics pipeline!");
                }

                // --- Cleanup Shader Modules ---
                vkDestroyShaderModule(g_Device, compositeVertModule, nullptr);
                vkDestroyShaderModule(g_Device, compositeFragModule, nullptr);

                // --- 4. Allocate Compositing Descriptor Sets ---
                compositeDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
                std::vector<VkDescriptorSetLayout> compositeLayouts(SwapChain::MAX_FRAMES_IN_FLIGHT, compositeSetLayout); // Renamed var
                VkDescriptorSetAllocateInfo compositeAllocInfo{};                                                         // Renamed var
                compositeAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                compositeAllocInfo.descriptorPool = g_DescriptorPool;
                compositeAllocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain::MAX_FRAMES_IN_FLIGHT);
                compositeAllocInfo.pSetLayouts = compositeLayouts.data();

                if (vkAllocateDescriptorSets(g_Device, &compositeAllocInfo, compositeDescriptorSets.data()) != VK_SUCCESS)
                {
                    // Cleanup previously created composite resources
                    vkDestroyPipeline(g_Device, compositePipeline, nullptr);
                    vkDestroyPipelineLayout(g_Device, compositePipelineLayout, nullptr);
                    vkDestroyDescriptorSetLayout(g_Device, compositeSetLayout, nullptr);
                    throw std::runtime_error("failed to allocate compositing descriptor sets!");
                }
                std::cout << "Compositing Pipeline Initialized." << std::endl;
            } // End Compositing Pipeline Scope
        }
        catch (const std::exception &e)
        {
            std::cerr << "Kinesis Initialization Failed: " << e.what() << std::endl;
            // --- Perform partial cleanup for compositing resources if they were created ---
            if (compositePipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(g_Device, compositePipeline, nullptr);
            if (compositePipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(g_Device, compositePipelineLayout, nullptr);
            if (compositeSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(g_Device, compositeSetLayout, nullptr);
            // --- Add Material Buffer Cleanup ---
            materialBuffer.reset(); // Cleanup material buffer if initialization failed
            // --- End Material Buffer Cleanup ---
            if (mainRenderSystem)
            {
                delete mainRenderSystem;
                mainRenderSystem = nullptr;
            }
            if (globalSetLayout != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(g_Device, globalSetLayout, nullptr);
                globalSetLayout = VK_NULL_HANDLE;
            }
            uboBuffers.clear();
            materialBuffer.reset();
            gameObjects.clear();
            Kinesis::GBuffer::cleanup(); // Cleanup GBuffer
            if (Kinesis::GUI::raytracing_available)
                Kinesis::RayTracerManager::cleanup(); // Cleanup RT
            Kinesis::Window::cleanup();
            throw;
        }
    }

    // Main application loop
    bool run()
    {
        // Check if the window should close
        if (glfwWindowShouldClose(Kinesis::Window::window))
        {
            if (g_Device != VK_NULL_HANDLE)
                vkDeviceWaitIdle(g_Device);
            // --- Cleanup Compositing Resources ---
            if (compositePipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(g_Device, compositePipeline, nullptr);
            if (compositePipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(g_Device, compositePipelineLayout, nullptr);
            if (compositeSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(g_Device, compositeSetLayout, nullptr);
            // Descriptor sets freed with pool
            compositeDescriptorSets.clear();
            // --- End Compositing Cleanup ---
            if (Kinesis::GUI::raytracing_available)
                Kinesis::RayTracerManager::cleanup();
            Kinesis::GBuffer::cleanup(); // Cleanup GBuffer resources
            if (mainRenderSystem)
                delete mainRenderSystem;
            if (globalSetLayout != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(g_Device, globalSetLayout, nullptr);
            // Cleanup Material Buffer
            materialBuffer.reset();
            uboBuffers.clear();
            gameObjects.clear();
            Kinesis::Window::cleanup();
            return false;
        }
        else
        {
            // --- Input, Event Processing, GUI Update, Camera Update ---
            glfwPollEvents();
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            Kinesis::Keyboard::moveInPlaneXZ(frameTime, player);
            mainCamera.setViewYXZ(player.transform.translation, player.transform.rotation);
            Kinesis::GUI::update_imgui();
            float aspect = Kinesis::Renderer::getAspectRatio();
            mainCamera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 1000.f); // Increased far plane

            try
            {
                if (auto commandBuffer = Kinesis::Renderer::beginFrame())
                {
                    int frameIndex = Kinesis::Renderer::currentFrameIndex;

                    // --- Update Camera UBO ---
                    CameraBufferObject ubo{};
                    ubo.projection = mainCamera.getProjection();
                    ubo.view = mainCamera.getView();
                    ubo.inverseProjection = glm::inverse(ubo.projection);
                    ubo.inverseView = glm::inverse(ubo.view);
                    uboBuffers[frameIndex]->writeToBuffer(&ubo, sizeof(ubo));

                    // =========================
                    // Pass 1: G-Buffer Pass
                    // =========================
                    {
                        VkRenderPassBeginInfo gbufferRenderPassBeginInfo{};
                        gbufferRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                        gbufferRenderPassBeginInfo.renderPass = Kinesis::GBuffer::renderPass;
                        gbufferRenderPassBeginInfo.framebuffer = Kinesis::GBuffer::frameBuffer;
                        gbufferRenderPassBeginInfo.renderArea.offset = {0, 0};
                        gbufferRenderPassBeginInfo.renderArea.extent = Kinesis::GBuffer::extent;
                        std::array<VkClearValue, 5> gbufferClearValues{};
                        gbufferClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Position
                        gbufferClearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Normal
                        gbufferClearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Albedo (Clear to black)
                        gbufferClearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Properties
                        gbufferClearValues[4].depthStencil = {1.0f, 0}; // Depth
                        gbufferRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(gbufferClearValues.size());
                        gbufferRenderPassBeginInfo.pClearValues = gbufferClearValues.data();

                        vkCmdBeginRenderPass(commandBuffer, &gbufferRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                        VkViewport gbufferViewport{};
                        gbufferViewport.x = 0.0f;
                        gbufferViewport.y = 0.0f;
                        gbufferViewport.width = static_cast<float>(Kinesis::GBuffer::extent.width);
                        gbufferViewport.height = static_cast<float>(Kinesis::GBuffer::extent.height);
                        gbufferViewport.minDepth = 0.0f;
                        gbufferViewport.maxDepth = 1.0f;
                        vkCmdSetViewport(commandBuffer, 0, 1, &gbufferViewport);
                        VkRect2D gbufferScissor{{0, 0}, Kinesis::GBuffer::extent};
                        vkCmdSetScissor(commandBuffer, 0, 1, &gbufferScissor);

                        if (mainRenderSystem)
                        {
                            mainRenderSystem->renderGameObjects(commandBuffer, mainCamera, globalDescriptorSets[frameIndex]);
                        }
                        vkCmdEndRenderPass(commandBuffer);
                    }

                    // =========================
                    // Pass 2: Ray Tracing Pass (Conditional)
                    // =========================
                    bool raytracing_active = Kinesis::GUI::raytracing_available && Kinesis::GUI::enable_raytracing_pass; // Check both flags

                    if (raytracing_active)
                    {
                         // Barrier to transition RT output image layout before tracing
                         VkImageMemoryBarrier rtOutputBarrier{};
                         rtOutputBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                         rtOutputBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                         rtOutputBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                         rtOutputBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Assume read previously
                         rtOutputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                         rtOutputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                         rtOutputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                         rtOutputBarrier.image = Kinesis::RayTracerManager::rtOutput.image;
                         rtOutputBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                         vkCmdPipelineBarrier(commandBuffer,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                              0, 0, nullptr, 0, nullptr, 1, &rtOutputBarrier);

                        Kinesis::RayTracerManager::allocateAndUpdateRtDescriptorSet(Kinesis::RayTracerManager::tlas.structure, VK_NULL_HANDLE, 0);
                        Kinesis::RayTracerManager::bind(commandBuffer, globalDescriptorSets[frameIndex]);
                        Kinesis::RayTracerManager::traceRays(commandBuffer, Kinesis::GBuffer::extent.width, Kinesis::GBuffer::extent.height);
                    }


                    // =========================
                    // Pass 3: Compositing Pass (Onto Swapchain)
                    // =========================
                    {
                        // --- Barrier: Wait for RT Writes (if active) or GBuffer writes, before Compositing Shader Reads ---
                        VkImageMemoryBarrier rtOutputToSampleBarrier{};
                        if (raytracing_active)
                        {
                            rtOutputToSampleBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                            rtOutputToSampleBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                            rtOutputToSampleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                            rtOutputToSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                            rtOutputToSampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            rtOutputToSampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            rtOutputToSampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            rtOutputToSampleBarrier.image = Kinesis::RayTracerManager::rtOutput.image;
                            rtOutputToSampleBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                        }

                        vkCmdPipelineBarrier(commandBuffer,
                                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | (raytracing_active ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : 0),
                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                             0, 0, nullptr, 0, nullptr,
                                             raytracing_active ? 1 : 0,
                                             raytracing_active ? &rtOutputToSampleBarrier : nullptr);

                        // --- Update Compositing Descriptor Set ---
                        VkDescriptorImageInfo posInfo{GBuffer::sampler, GBuffer::positionAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                        VkDescriptorImageInfo normInfo{GBuffer::sampler, GBuffer::normalAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                        VkDescriptorImageInfo albInfo{GBuffer::sampler, GBuffer::albedoAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                        VkDescriptorImageInfo propInfo{GBuffer::sampler, GBuffer::propertiesAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                        VkDescriptorImageInfo rtInfo{};

                        if (raytracing_active)
                        {
                            if (RayTracerManager::rtOutput.view == VK_NULL_HANDLE) {
                                 std::cerr << "Warning: Raytracing active but rtOutput.view is null. Using fallback." << std::endl;
                                 if (GBuffer::albedoAttachment.view == VK_NULL_HANDLE) throw std::runtime_error("Fallback G-Buffer view (albedo) is also null!");
                                 rtInfo = {GBuffer::sampler, GBuffer::albedoAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                            } else {
                                 rtInfo = {GBuffer::sampler, RayTracerManager::rtOutput.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                            }
                        }
                        else
                        {
                            if (GBuffer::albedoAttachment.view == VK_NULL_HANDLE) throw std::runtime_error("Fallback G-Buffer view (albedo) is null!");
                            rtInfo = {GBuffer::sampler, GBuffer::albedoAttachment.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                        }

                        std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
                        descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, compositeDescriptorSets[frameIndex], 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &posInfo, nullptr, nullptr };
                        descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, compositeDescriptorSets[frameIndex], 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normInfo, nullptr, nullptr };
                        descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, compositeDescriptorSets[frameIndex], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albInfo, nullptr, nullptr };
                        descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, compositeDescriptorSets[frameIndex], 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &propInfo, nullptr, nullptr };
                        descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, compositeDescriptorSets[frameIndex], 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &rtInfo, nullptr, nullptr };
                        vkUpdateDescriptorSets(g_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

                        // --- Begin Swapchain Render Pass ---
                        Renderer::beginSwapChainRenderPass(commandBuffer);

                        // --- Bind Compositing Pipeline & Descriptors ---
                        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline);
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipelineLayout,
                                                0, 1, &compositeDescriptorSets[frameIndex], 0, nullptr);

                        // <<< ADDED: Push the 'raytracing_active' flag to the shader >>>
                        CompositePushConstant pushData = { raytracing_active ? 1 : 0 };
                        vkCmdPushConstants(
                            commandBuffer,
                            compositePipelineLayout,    // The layout associated with the composite pipeline
                            VK_SHADER_STAGE_FRAGMENT_BIT, // Stage where the constant is used
                            0,                            // Offset within the push constant block
                            sizeof(CompositePushConstant),// Size of the data being pushed
                            &pushData);                   // Pointer to the data

                        // --- Draw Fullscreen Triangle ---
                        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

                        // --- Render ImGui ---
                        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

                        // --- End Swapchain Render Pass ---
                        Renderer::endSwapChainRenderPass(commandBuffer);
                    } // End Compositing Pass Scope

                    // --- End Frame ---
                    Renderer::endFrame();
                } // End if(commandBuffer)
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error during rendering loop: " << e.what() << std::endl;
                 // return false; // Exit loop on error
            }
            return true; // Signal loop continuation
        } // End else (window not closing)
    } // End run()

    // --- Helper functions for creating simple models ---
    // Uses Model::Builder now

    std::shared_ptr<Model> createCubeModel(glm::vec3 offset)
    {
        // Define unique vertices for a cube
        std::vector<Mesh::Vertex> vertices = {
            // Front face
            Mesh::Vertex(0, {-0.5f, -0.5f, 0.5f}, {0.1f, 0.1f, 0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}), // Bottom-left
            Mesh::Vertex(1, {0.5f, -0.5f, 0.5f}, {0.1f, 0.1f, 0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}),  // Bottom-right
            Mesh::Vertex(2, {0.5f, 0.5f, 0.5f}, {0.1f, 0.1f, 0.8f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}),   // Top-right
            Mesh::Vertex(3, {-0.5f, 0.5f, 0.5f}, {0.1f, 0.1f, 0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}),  // Top-left
            // Back face
            Mesh::Vertex(4, {-0.5f, -0.5f, -0.5f}, {0.1f, 0.8f, 0.1f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}), // Bottom-left
            Mesh::Vertex(5, {0.5f, -0.5f, -0.5f}, {0.1f, 0.8f, 0.1f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}),  // Bottom-right
            Mesh::Vertex(6, {0.5f, 0.5f, -0.5f}, {0.1f, 0.8f, 0.1f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),   // Top-right
            Mesh::Vertex(7, {-0.5f, 0.5f, -0.5f}, {0.1f, 0.8f, 0.1f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}),  // Top-left
            // Left face
            Mesh::Vertex(8, {-0.5f, -0.5f, -0.5f}, {0.9f, 0.9f, 0.9f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}), // Bottom-front
            Mesh::Vertex(9, {-0.5f, -0.5f, 0.5f}, {0.9f, 0.9f, 0.9f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}),  // Bottom-back
            Mesh::Vertex(10, {-0.5f, 0.5f, 0.5f}, {0.9f, 0.9f, 0.9f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}),  // Top-back
            Mesh::Vertex(11, {-0.5f, 0.5f, -0.5f}, {0.9f, 0.9f, 0.9f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}), // Top-front
            // Right face
            Mesh::Vertex(12, {0.5f, -0.5f, -0.5f}, {0.8f, 0.8f, 0.1f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}), // Bottom-front
            Mesh::Vertex(13, {0.5f, -0.5f, 0.5f}, {0.8f, 0.8f, 0.1f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}),  // Bottom-back
            Mesh::Vertex(14, {0.5f, 0.5f, 0.5f}, {0.8f, 0.8f, 0.1f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}),   // Top-back
            Mesh::Vertex(15, {0.5f, 0.5f, -0.5f}, {0.8f, 0.8f, 0.1f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}),  // Top-front
            // Top face (y is up)
            Mesh::Vertex(16, {-0.5f, 0.5f, 0.5f}, {0.8f, 0.1f, 0.1f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),  // Front-left
            Mesh::Vertex(17, {0.5f, 0.5f, 0.5f}, {0.8f, 0.1f, 0.1f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),   // Front-right
            Mesh::Vertex(18, {0.5f, 0.5f, -0.5f}, {0.8f, 0.1f, 0.1f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),  // Back-right
            Mesh::Vertex(19, {-0.5f, 0.5f, -0.5f}, {0.8f, 0.1f, 0.1f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}), // Back-left
            // Bottom face (y is down)
            Mesh::Vertex(20, {-0.5f, -0.5f, 0.5f}, {0.9f, 0.6f, 0.1f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}), // Front-left
            Mesh::Vertex(21, {0.5f, -0.5f, 0.5f}, {0.9f, 0.6f, 0.1f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}),  // Front-right
            Mesh::Vertex(22, {0.5f, -0.5f, -0.5f}, {0.9f, 0.6f, 0.1f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}), // Back-right
            Mesh::Vertex(23, {-0.5f, -0.5f, -0.5f}, {0.9f, 0.6f, 0.1f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}) // Back-left
        };

        for (auto &v : vertices)
        {
            v.position += offset;
        }

        // Define indices for the cube (6 faces, 2 triangles per face)
        std::vector<uint32_t> indices = {
            0, 1, 2, 0, 2, 3,       // Front
            4, 5, 6, 4, 6, 7,       // Back
            8, 9, 10, 8, 10, 11,    // Left
            12, 13, 14, 12, 14, 15, // Right
            16, 17, 18, 16, 18, 19, // Top
            20, 21, 22, 20, 22, 23  // Bottom
        };

        Model::Builder builder{};
        builder.vertices = vertices;
        builder.indices = indices; // Add indices to builder
        return std::make_shared<Model>(builder);
    }

    // Load initial game scene data
    void loadGameObjects()
    {
        std::shared_ptr<Model> mod_obj = nullptr;
        std::shared_ptr<Model> mod_obj2 = nullptr;
        try
        {
            // Adjust path based on execution directory relative to assets
            std::string modelPath = "../../../kinesis/assets/models"; // Common relative path
#ifdef __APPLE__
             modelPath = "../../../../../../kinesis/assets/models";
#endif
            mod_obj = std::make_shared<Model>(modelPath, "bunny_200.obj");
            mod_obj2 = std::make_shared<Model>(modelPath, "cornell_box.obj");
            // mod_obj = std::make_shared<Model>(modelPath, "smooth_vase.obj");
            // mod_obj = createCubeModel({0.f, 0.f, 0.f}); // Use cube for testing
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "Failed to load OBJ model: " << e.what() << std::endl;
            mod_obj = createCubeModel({0.f, 0.f, 2.5f}); // Fallback
        }

        GameObject bunny = GameObject::createGameObject("bunny");
        bunny.model = mod_obj;
        bunny.transform.translation = {0.f, -2.0f, 2.5f}; // Adjusted y
        bunny.transform.scale = {1.0f, 1.0f, 1.0f};
        bunny.transform.rotation = {0.f, 0.f, 0.f};
        gameObjects.push_back(std::move(bunny));

        GameObject cornell = GameObject::createGameObject("cornell");
        cornell.model = mod_obj2;
        cornell.transform.translation = {0.f, -0.5f, 2.5f}; // Adjusted y
        cornell.transform.scale = {1.0f, 1.0f, 1.0f};
        cornell.transform.rotation = {0.f, 0.f, 0.f};
        gameObjects.push_back(std::move(cornell));
    }

} // namespace Kinesis