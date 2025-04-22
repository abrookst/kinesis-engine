#include "kinesis.h"
#include "buffer.h" // Include the new Buffer helper

// Define structure matching shader uniform buffer object (UBO)
// Ensure layout matches shader definition (std140 alignment often used)
struct CameraBufferObject {
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 inverseProjection; // Optional: For reconstructing world pos from depth
    alignas(16) glm::mat4 inverseView;       // Optional: For world space calculations
    // Add other global uniforms like camera position if needed
};

#include "window.h"
#include "GUI.h"
#include "renderer.h"
#include "rendersystem.h" // Include RenderSystem header
#include "camera.h"
#include "keyboard_controller.h"
#include <iostream> // For std::cerr
#include <stdexcept> // For std::exception
#include <vector>    // For std::vector
#include <memory>    // For std::unique_ptr, std::make_unique, std::shared_ptr
#include <cassert>   // For assert
#include <chrono>


using namespace Kinesis;

namespace Kinesis {
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
    RenderSystem* mainRenderSystem = nullptr;
    Camera mainCamera = Camera();
    GameObject player  = GameObject::createGameObject("player");

    std::vector<GameObject> gameObjects = std::vector<GameObject>();

    // --- Additions for Global Descriptor Set ---
    std::vector<std::unique_ptr<Buffer>> uboBuffers;
    VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> globalDescriptorSets;
    // --- End Additions ---

    auto currentTime = std::chrono::high_resolution_clock::now();

    void initialize(int width, int height){
        try {
            Kinesis::Window::initialize(width, height); // Initializes Vulkan core, window, ImGui
            loadGameObjects(); // Loads models into gameObjects vector

            // --- Create Global UBO Buffers & Descriptor Set Layout/Sets ---
            // One UBO buffer per frame in flight
            uboBuffers.resize(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            for (int i = 0; i < uboBuffers.size(); ++i) {
                uboBuffers[i] = std::make_unique<Buffer>(
                    sizeof(CameraBufferObject), // Use the defined struct size
                    1, // Only one instance of the global UBO
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, // Usage is UBO
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // Host visible/coherent for easy CPU updates
                );
                uboBuffers[i]->map(); // Map the buffer persistently
            }

            // Create Descriptor Set Layout (defines the structure of the set)
            VkDescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0; // Corresponds to layout(set=0, binding=0) in shaders
            uboLayoutBinding.descriptorCount = 1; // One UBO
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.pImmutableSamplers = nullptr;
            // Make the UBO accessible to relevant shader stages
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &uboLayoutBinding;

            if (vkCreateDescriptorSetLayout(g_Device, &layoutInfo, nullptr, &globalSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create global descriptor set layout!");
            }

            // Allocate Descriptor Sets (one per frame in flight) from the global pool
            globalDescriptorSets.resize(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            std::vector<VkDescriptorSetLayout> layouts(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT, globalSetLayout); // Need layout array for allocation
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = g_DescriptorPool; // Use the global pool created in window.cpp
            allocInfo.descriptorSetCount = static_cast<uint32_t>(Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT);
            allocInfo.pSetLayouts = layouts.data();

            if (vkAllocateDescriptorSets(g_Device, &allocInfo, globalDescriptorSets.data()) != VK_SUCCESS) {
                 throw std::runtime_error("failed to allocate global descriptor sets!");
            }

            // Update Descriptor Sets to point them to the corresponding UBO buffers
            for (int i = 0; i < globalDescriptorSets.size(); i++) {
                auto bufferInfo = uboBuffers[i]->descriptorInfo(); // Get VkDescriptorBufferInfo for the buffer
                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = globalDescriptorSets[i]; // Target the specific set for this frame index
                descriptorWrite.dstBinding = 0; // Matches the layout binding
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo; // Point to the buffer info

                vkUpdateDescriptorSets(g_Device, 1, &descriptorWrite, 0, nullptr); // Update the set
            }
            // --- End Descriptor Set Setup ---

            // RenderSystem constructor needs the globalSetLayout to be ready if it uses it
            mainRenderSystem = new RenderSystem(); // RenderSystem constructor calls createPipelineLayout/createPipeline

        } catch (const std::exception& e) {
            std::cerr << "Kinesis Initialization Failed: " << e.what() << std::endl;
            // Perform partial cleanup
             if (mainRenderSystem) {
                 delete mainRenderSystem;
                 mainRenderSystem = nullptr;
             }
             // Cleanup descriptor set layout if created
             if (globalSetLayout != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE) {
                 vkDestroyDescriptorSetLayout(g_Device, globalSetLayout, nullptr);
                 globalSetLayout = VK_NULL_HANDLE;
             }
             // Buffers cleaned by unique_ptr
             uboBuffers.clear();
             // Window::cleanup handles core Vulkan/GLFW/ImGui cleanup
             Kinesis::Window::cleanup();
             throw; // Re-throw to signal failure
        }
    }

    // Main application loop
    bool run()
    {

         // Check if the window should close
        if(glfwWindowShouldClose(Kinesis::Window::window)){
            // --- Cleanup before exiting ---
            try {
                // Ensure device is idle before destroying resources
                if (g_Device != VK_NULL_HANDLE) {
                     vkDeviceWaitIdle(g_Device);
                }
                // Destroy application-specific resources
                if (mainRenderSystem) {
                    delete mainRenderSystem;
                    mainRenderSystem = nullptr;
                }
                 // Clean up descriptor set layout
                if (globalSetLayout != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE) {
                     vkDestroyDescriptorSetLayout(g_Device, globalSetLayout, nullptr);
                     globalSetLayout = VK_NULL_HANDLE;
                }
                // UBO Buffers are cleaned up automatically by unique_ptr
                uboBuffers.clear();
                // Descriptor sets are automatically freed when the pool (g_DescriptorPool) is destroyed
                globalDescriptorSets.clear();

                // Clear game objects (models will be destroyed by shared_ptr if no other references exist)
                gameObjects.clear();

                // Call global cleanup function (cleans up Window, Renderer, Vulkan core, GLFW, ImGui)
                Kinesis::Window::cleanup();
            } catch (const std::exception& e) {
                 std::cerr << "Error during cleanup: " << e.what() << std::endl;
            }
            return false; // Signal loop termination
        }
        else{
            // --- Input and Event Processing ---
            glfwPollEvents();
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime-currentTime).count();
            currentTime = newTime;

            // Update player movement/camera based on input
            Kinesis::Keyboard::moveInPlaneXZ(frameTime, player);
            mainCamera.setViewYXZ(player.transform.translation, player.transform.rotation);

            // --- GUI Update ---
            Kinesis::GUI::update_imgui(); // Prepare ImGui frame data

            // --- Rendering ---
            try {
                 // Start the frame: acquire swapchain image, begin command buffer
                float aspect = Kinesis::Renderer::getAspectRatio();
                mainCamera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f); // Increased far plane

                 if(auto commandBuffer = Kinesis::Renderer::beginFrame()){ // Returns null if swapchain needs recreate

                    // --- Update Camera UBO ---
                    int frameIndex = Kinesis::Renderer::currentFrameIndex; // Get current frame index from Renderer
                    CameraBufferObject ubo{}; // Create UBO data struct on stack
                    ubo.projection = mainCamera.getProjection();
                    ubo.view = mainCamera.getView();
                    // Calculate inverse matrices (useful for world pos reconstruction, etc.)
                    ubo.inverseProjection = glm::inverse(ubo.projection);
                    ubo.inverseView = glm::inverse(ubo.view);

                    // Write UBO data to the mapped buffer for the current frame
                    uboBuffers[frameIndex]->writeToBuffer(&ubo, sizeof(ubo));
                    // uboBuffers[frameIndex]->flush(); // Only needed if memory is not HOST_COHERENT

                    // --- Begin Render Pass ---
                    // This begins the *swapchain* render pass (for final output to screen)
                    // G-Buffer rendering might happen before this in a separate pass, or directly here if simple.
                    // Assuming G-Buffer pass happens first, then composition/ImGui in swapchain pass.
                    // TODO: Implement G-Buffer rendering pass if deferred shading is used.
                    // For now, render directly to swapchain render pass.

                    Kinesis::Renderer::beginSwapChainRenderPass(commandBuffer); // Use the main swapchain render pass

                    // --- Render Game Objects ---
                    if (mainRenderSystem) {
                        // Pass the correct global descriptor set for the current frame
                        // This renders using the pipeline bound by RenderSystem (currently GBuffer pipeline)
                        // This should likely render to the G-Buffer framebuffer, not the swapchain one.
                        // We need to adjust where renderGameObjects is called or what renderpass/pipeline it uses.

                        // **Temporary:** Render directly to swapchain pass using the RenderSystem's pipeline
                        mainRenderSystem->renderGameObjects(commandBuffer, mainCamera, globalDescriptorSets[frameIndex]);
                    }

                     // --- Render ImGui ---
                    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

                    // --- End Render Pass ---
                    Kinesis::Renderer::endSwapChainRenderPass(commandBuffer);

                    // --- End Frame ---
                    Kinesis::Renderer::endFrame(); // Submits command buffer, presents image
                }
            } catch (const std::exception& e) {
                std::cerr << "Error during rendering loop: " << e.what() << std::endl;
                 // Consider how to handle errors - continue, break, rethrow?
            }
            return true; // Signal loop continuation
        }
    }

    // --- Helper functions for creating simple models ---
    // Uses Model::Builder now

    std::shared_ptr<Model> createCubeModel(glm::vec3 offset) {
        std::vector<Mesh::Vertex> vertices{
       
            // left face (white) - Indices 0 to 5
            Mesh::Vertex(0, {-.5f, -.5f, -.5f}, {.9f, .9f, .9f}),
            Mesh::Vertex(1, {-.5f,  .5f,  .5f}, {.9f, .9f, .9f}),
            Mesh::Vertex(2, {-.5f, -.5f,  .5f}, {.9f, .9f, .9f}),
            // Note: Original vertices list seemed to have duplicates, ensure correct vertices for a cube.
            // Assuming unique vertices per face corner for simplicity here. Adjust if needed.
            // Re-using vertex 0, 1, 2 for the second triangle of the left face
            Mesh::Vertex(3, {-.5f, -.5f, -.5f}, {.9f, .9f, .9f}), // Same as 0
            Mesh::Vertex(4, {-.5f,  .5f, -.5f}, {.9f, .9f, .9f}),
            Mesh::Vertex(5, {-.5f,  .5f,  .5f}, {.9f, .9f, .9f}), // Same as 1

            // right face (yellow) - Indices 6 to 11
            Mesh::Vertex(6, {.5f, -.5f, -.5f}, {.8f, .8f, .1f}),
            Mesh::Vertex(7, {.5f,  .5f,  .5f}, {.8f, .8f, .1f}),
            Mesh::Vertex(8, {.5f, -.5f,  .5f}, {.8f, .8f, .1f}),
            Mesh::Vertex(9, {.5f, -.5f, -.5f}, {.8f, .8f, .1f}), // Same as 6
            Mesh::Vertex(10,{ .5f,  .5f, -.5f}, {.8f, .8f, .1f}),
            Mesh::Vertex(11,{ .5f,  .5f,  .5f}, {.8f, .8f, .1f}), // Same as 7

            // top face (orange, y points down) - Indices 12 to 17
            Mesh::Vertex(12,{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}),
            Mesh::Vertex(13,{ .5f, -.5f,  .5f}, {.9f, .6f, .1f}),
            Mesh::Vertex(14,{-.5f, -.5f,  .5f}, {.9f, .6f, .1f}),
            Mesh::Vertex(15,{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}), // Same as 12
            Mesh::Vertex(16,{ .5f, -.5f, -.5f}, {.9f, .6f, .1f}),
            Mesh::Vertex(17,{ .5f, -.5f,  .5f}, {.9f, .6f, .1f}), // Same as 13

            // bottom face (red) - Indices 18 to 23
            Mesh::Vertex(18,{-.5f,  .5f, -.5f}, {.8f, .1f, .1f}),
            Mesh::Vertex(19,{ .5f,  .5f,  .5f}, {.8f, .1f, .1f}),
            Mesh::Vertex(20,{-.5f,  .5f,  .5f}, {.8f, .1f, .1f}),
            Mesh::Vertex(21,{-.5f,  .5f, -.5f}, {.8f, .1f, .1f}), // Same as 18
            Mesh::Vertex(22,{ .5f,  .5f, -.5f}, {.8f, .1f, .1f}),
            Mesh::Vertex(23,{ .5f,  .5f,  .5f}, {.8f, .1f, .1f}), // Same as 19

            // nose face (blue) - Indices 24 to 29
            Mesh::Vertex(24,{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}),
            Mesh::Vertex(25,{ .5f,  .5f, 0.5f}, {.1f, .1f, .8f}),
            Mesh::Vertex(26,{-.5f,  .5f, 0.5f}, {.1f, .1f, .8f}),
            Mesh::Vertex(27,{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}), // Same as 24
            Mesh::Vertex(28,{ .5f, -.5f, 0.5f}, {.1f, .1f, .8f}),
            Mesh::Vertex(29,{ .5f,  .5f, 0.5f}, {.1f, .1f, .8f}), // Same as 25

            // tail face (green) - Indices 30 to 35
            Mesh::Vertex(30,{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}),
            Mesh::Vertex(31,{ .5f,  .5f, -0.5f}, {.1f, .8f, .1f}),
            Mesh::Vertex(32,{-.5f,  .5f, -0.5f}, {.1f, .8f, .1f}),
            Mesh::Vertex(33,{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}), // Same as 30
            Mesh::Vertex(34,{ .5f, -.5f, -0.5f}, {.1f, .8f, .1f}),
            Mesh::Vertex(35,{ .5f,  .5f, -0.5f}, {.1f, .8f, .1f}), // Same as 31
        };
        for (auto& v : vertices) {
          v.position += offset;
        }
        Model::Builder builder{};
        builder.vertices = vertices;
        return std::make_shared<Model>(builder); // Use make_shared if intended
      }

      // Note: createCornellBox needs similar update to use unique vertices and indices
      // For brevity, updating only createCubeModel here. Apply similar logic if using createCornellBox.
	std::shared_ptr<Model> createCornellBox() {
         // Similar structure: define unique vertices, then indices
         // Using simplified vertices for now - replace with actual cornell box setup
         std::vector<Mesh::Vertex> vertices{
       
            // left face (red) - Indices 0 to 5
            Mesh::Vertex(0, {-.5f, -.5f, -.5f}, {1.f, 0.f, 0.f}),
            Mesh::Vertex(1, {-.5f,  .5f,  .5f}, {1.f, 0.f, 0.f}),
            Mesh::Vertex(2, {-.5f, -.5f,  .5f}, {1.f, 0.f, 0.f}),
            Mesh::Vertex(3, {-.5f, -.5f, -.5f}, {1.f, 0.f, 0.f}), // Same as 0
            Mesh::Vertex(4, {-.5f,  .5f, -.5f}, {1.f, 0.f, 0.f}),
            Mesh::Vertex(5, {-.5f,  .5f,  .5f}, {1.f, 0.f, 0.f}), // Same as 1

            // right face (green) - Indices 6 to 11
            Mesh::Vertex(6, {.5f, -.5f, -.5f}, {0.f, 1.f, 0.f}),
            Mesh::Vertex(7, {.5f,  .5f,  .5f}, {0.f, 1.f, 0.f}),
            Mesh::Vertex(8, {.5f, -.5f,  .5f}, {0.f, 1.f, 0.f}),
            Mesh::Vertex(9, {.5f, -.5f, -.5f}, {0.f, 1.f, 0.f}), // Same as 6
            Mesh::Vertex(10,{ .5f,  .5f, -.5f}, {0.f, 1.f, 0.f}),
            Mesh::Vertex(11,{ .5f,  .5f,  .5f}, {0.f, 1.f, 0.f}), // Same as 7

            // top face (white) - Indices 12 to 17
            Mesh::Vertex(12,{-.5f, -.5f, -.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(13,{ .5f, -.5f,  .5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(14,{-.5f, -.5f,  .5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(15,{-.5f, -.5f, -.5f}, {.84f, .84f, .84f}), // Same as 12
            Mesh::Vertex(16,{ .5f, -.5f, -.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(17,{ .5f, -.5f,  .5f}, {.84f, .84f, .84f}), // Same as 13

            // bottom face (white) - Indices 18 to 23
            Mesh::Vertex(18,{-.5f,  .5f, -.5f}, {.54f, .54f, .54f}),
            Mesh::Vertex(19,{ .5f,  .5f,  .5f}, {.54f, .54f, .54f}),
            Mesh::Vertex(20,{-.5f,  .5f,  .5f}, {.54f, .54f, .54f}),
            Mesh::Vertex(21,{-.5f,  .5f, -.5f}, {.54f, .54f, .54f}), // Same as 18
            Mesh::Vertex(22,{ .5f,  .5f, -.5f}, {.54f, .54f, .54f}),
            Mesh::Vertex(23,{ .5f,  .5f,  .5f}, {.54f, .54f, .54f}), // Same as 19

            // nose face (white) - Indices 24 to 29
            Mesh::Vertex(24,{-.5f, -.5f, 0.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(25,{ .5f,  .5f, 0.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(26,{-.5f,  .5f, 0.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(27,{-.5f, -.5f, 0.5f}, {.84f, .84f, .84f}), // Same as 24
            Mesh::Vertex(28,{ .5f, -.5f, 0.5f}, {.84f, .84f, .84f}),
            Mesh::Vertex(29,{ .5f,  .5f, 0.5f}, {.84f, .84f, .84f}), // Same as 25
         };

        Model::Builder builder{};
        builder.vertices = vertices;
        return std::make_shared<Model>(builder); // Use make_shared if intended
      }

    // Load initial game scene data
    void loadGameObjects()
    {
         // Create models using make_shared or make_unique as appropriate
        // std::shared_ptr<Model> mod_cube = createCubeModel({0.f, 0.f, 2.5f}); // Example Cube

        // Load OBJ model - ensure path is correct relative to executable
        std::shared_ptr<Model> mod_obj = nullptr;
        try {
            // Adjust path as needed - this path is relative to the build output usually
            #ifdef _WIN32
            mod_obj = std::make_shared<Model>("../../kinesis/assets/models", "bunny_1k.obj"); // Example path for Windows build
            #else
             mod_obj = std::make_shared<Model>("../kinesis/assets/models", "bunny_1k.obj"); // Example path for other systems
             // Alternatively use absolute paths or configure paths better
             #endif
        } catch (const std::runtime_error& e) {
             std::cerr << "Failed to load OBJ model: " << e.what() << std::endl;
             // Handle error - maybe load a default model or exit?
             // For now, create a fallback cube
             mod_obj = createCubeModel({0.f, 0.f, 2.5f});
        }


        // Create game object using the loaded model
        GameObject bunny = GameObject::createGameObject("bunny");
        bunny.model = mod_obj; // Assign the model
        bunny.transform.translation = {0.f, 0.0f, 2.5f}; // Adjust position
        bunny.transform.scale = {1.0f, 1.0f, 1.0f};      // Adjust scale
        bunny.transform.rotation = {0.f, glm::radians(180.f), 0.f}; // Adjust rotation
        gameObjects.push_back(std::move(bunny));

		// Optional: Add Cornell Box or other objects
		// std::shared_ptr<Model> mod_cornell = createCornellBox();
		// GameObject cornell_box = GameObject::createGameObject("cornell_box");
		// cornell_box.model = mod_cornell;
		// cornell_box.transform.translation = {0.f,0.f,2.5f};
		// cornell_box.transform.scale = {2.f,2.f,2.f};
		// gameObjects.push_back(std::move(cornell_box));
    }

} // namespace Kinesis