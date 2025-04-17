#include "kinesis.h"
#include "window.h"
#include "GUI.h"
#include "renderer.h"
#include "rendersystem.h" // Include RenderSystem header
#include <iostream> // For std::cerr
#include <stdexcept> // For std::exception
#include <vector>    // For std::vector
#include <memory>    // For std::shared_ptr
#include <cassert>   // For assert


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
    std::vector<GameObject> gameObjects = std::vector<GameObject>();


    void initialize(int width, int height){
        try {
            // 1. Initialize Window and Core Vulkan (calls Renderer::Initialize internally now)
            std::cout << "1" << std::endl;
            Kinesis::Window::initialize(width, height);
            std::cout << "2" << std::endl;
            loadGameObjects();
            std::cout << "3" << std::endl;
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

            // --- GUI Update ---
            Kinesis::GUI::update_imgui(); // Prepare ImGui frame data

            // --- Rendering ---
            try {
                 // Start the frame: acquire swapchain image, begin command buffer
                if(auto commandBuffer = Kinesis::Renderer::beginFrame()){
                    Kinesis::Renderer::beginSwapChainRenderPass(commandBuffer);
                    if (mainRenderSystem) {
                        mainRenderSystem->renderGameObjects(commandBuffer);
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

    // Load initial game scene data
    void loadGameObjects()
    {
        std::vector<Vertex> vertices = {
             // Position                 Color
             Vertex(0, { 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}), // Vertex 0: Bottom, Red
             Vertex(1, { 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}), // Vertex 1: Top right, Green
             Vertex(2, {-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f})  // Vertex 2: Top left, Blue
        };


         // Ensure device is ready before creating model (which creates buffers)
        assert(g_Device != VK_NULL_HANDLE && "Device must be initialized before loading game objects");

        // Create a shared pointer to the model
        std::shared_ptr<Model> triModel = nullptr;
        try {
             triModel = std::make_shared<Model>(vertices);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create model: " << e.what() << std::endl;
            throw; // Rethrow, as rendering requires the model
        }


        // Create a GameObject using the factory method
        GameObject triangle = GameObject::createGameObject("Test Triangle");
        triangle.model = triModel; // Assign the model
        triangle.color = glm::vec3(0.8f, 0.1f, 0.1f); // Set object color (used by push constant)
        triangle.transform.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f)); // Center the triangle initially
        triangle.transform.SetScale(glm::vec3(1.0f, 2.0f, 1.0f)); // Set scale to 1
        triangle.transform.SetRotation(0.0f); // No rotation initially

        // Add the game object to the global list using move semantics
        gameObjects.push_back(std::move(triangle));


        std::cout << "Loaded " << gameObjects.size() << " game object(s)." << std::endl;
    }

} // namespace Kinesis