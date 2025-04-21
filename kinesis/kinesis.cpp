#include "kinesis.h"
#include "window.h"
#include "GUI.h"
#include "renderer.h"
#include "rendersystem.h" // Include RenderSystem header
#include "camera.h"
#include "keyboard_controller.h"
#include <iostream> // For std::cerr
#include <stdexcept> // For std::exception
#include <vector>    // For std::vector
#include <memory>    // For std::shared_ptr
#include <cassert>   // For assert
#include <chrono>


using namespace Kinesis;

namespace Kinesis {
    // Global Vulkan Objects (defined in window.cpp or elsewhere)
    // Ensure these are initialized before use.
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
    auto currentTime = std::chrono::high_resolution_clock::now();

    void initialize(int width, int height){
        try {
            Kinesis::Window::initialize(width, height);
            loadGameObjects();
             assert(Kinesis::Renderer::SwapChain != nullptr && "Renderer/SwapChain must be initialized before creating RenderSystem");
            mainRenderSystem = new RenderSystem(); // RenderSystem constructor now calls createPipeline

        } catch (const std::exception& e) {
            std::cerr << "Kinesis Initialization Failed: " << e.what() << std::endl;
            // Perform partial cleanup if possible, then exit or rethrow
             if (mainRenderSystem) {
                 delete mainRenderSystem;
                 mainRenderSystem = nullptr;
             }
             // Window::cleanup() handles Vulkan/GLFW/ImGui cleanup
             Kinesis::Window::cleanup();
             throw; // Re-throw to signal failure to the caller
        }
    }

    // Main application loop
    bool run()
    {
        
         // Check if the window should close (e.g., user clicked the close button)
        if(glfwWindowShouldClose(Kinesis::Window::window)){
            // Perform cleanup before exiting
            try {
                // Ensure device is idle before destroying resources in cleanup
                if (g_Device != VK_NULL_HANDLE) {
                     vkDeviceWaitIdle(g_Device);
                }
                if (mainRenderSystem) {
                    delete mainRenderSystem; // Destroy render system
                    mainRenderSystem = nullptr;
                }
                gameObjects.clear();

                Kinesis::Window::cleanup(); // Cleans up Window, Renderer, Vulkan, GLFW, ImGui
            } catch (const std::exception& e) {
                 std::cerr << "Error during cleanup: " << e.what() << std::endl;
                 // Exit even if cleanup failed partially
            }
            return false; // Signal loop termination
        }
        else{
            // --- Input and Event Processing ---
            glfwPollEvents(); // Process window events (input, resize, etc.)
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime-currentTime).count();
            currentTime = newTime;
            Kinesis::Keyboard::moveInPlaneXZ(frameTime, player);

            mainCamera.setViewYXZ(player.transform.translation, player.transform.rotation);

            // --- GUI Update ---
            Kinesis::GUI::update_imgui(); // Prepare ImGui frame data

            // --- Rendering ---
            try {
                 // Start the frame: acquire swapchain image, begin command buffer
                float aspect = Kinesis::Renderer::getAspectRatio();
                //mainCamera.setOrthographicProjection(-aspect,aspect,-1,1,-1,1);
                mainCamera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

                 if(auto commandBuffer = Kinesis::Renderer::beginFrame()){
                    Kinesis::Renderer::beginSwapChainRenderPass(commandBuffer);
                    if (mainRenderSystem) {
                        mainRenderSystem->renderGameObjects(commandBuffer, mainCamera);
                    }
                    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

                    Kinesis::Renderer::endSwapChainRenderPass(commandBuffer);
                    Kinesis::Renderer::endFrame();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error during rendering loop: " << e.what() << std::endl;
            }
            return true; // Signal loop continuation
        }
    }

    std::unique_ptr<Model> createCubeModel(glm::vec3 offset) {
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
        return std::make_unique<Model>(vertices);
      }

    // Load initial game scene data
    void loadGameObjects()
    {
        // std::shared_ptr<Model> mod = createCubeModel({0.f,0.f,0.f});
		// std::shared_ptr<Model> mod = std::make_unique<Model>("../../kinesis/assets/models", "cornell_box.obj");
		std::shared_ptr<Model> mod = std::make_unique<Model>("../../kinesis/assets/models", "bunny_200.obj");

        GameObject cube = GameObject::createGameObject("bunny");
        cube.model = mod;
        cube.transform.translation = {0.f,0.5f,2.5f};
        cube.transform.scale = {-2.f,-2.f,-2.f};
        gameObjects.push_back(std::move(cube));
    }

} // namespace Kinesis
