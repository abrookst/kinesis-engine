#ifndef WINDOW_H
#define WINDOW_H

#include <memory>
#include <vector>
#include <array>

#include "kinesis.h"
#include "model.h"
#include "pipeline.h"
#include "swapchain.h"



namespace Kinesis::Window
{
    extern GLFWwindow *window;
    extern VkPipelineLayout pipelineLayout;
    extern std::vector<VkCommandBuffer> commandBuffers;
    extern ImGui_ImplVulkanH_Window *wd;
    extern uint32_t width;
    extern uint32_t height;
    extern bool fbResized;

    /**
     * @brief Creates the Vulkan pipeline layout.
     */
    void createPipelineLayout();

    /**
     * @brief Rerenders the swapchain given a window resizing.
     */
    void recreateSwapchain();

    /**
     * @brief records the current command buffer
     * @param i the image index you wish to record.
     */
    void recordCommandBuffer(int i);

    /**
     * @brief Creates the graphics pipeline.
     */
    void createPipeline();

    /**
     * @brief Creates and records command buffers for rendering.
     */
    void createCommandBuffers();

    /**
     * @brief Acquires the next swapchain image, submits command buffers, and presents the frame.
     */
    void drawFrame();

    /**
     * @brief GLFW error callback function.
     * @param error GLFW error code.
     * @param description Human-readable error description.
     */
    void glfw_error_callback(int error, const char *description);

    /**
     * @brief Checks a Vulkan result code and prints an error message if it's not VK_SUCCESS.
     * Aborts the program if the result is a negative error code.
     * @param err The VkResult to check.
     */
    void check_vk_result(VkResult err);

    /**
     * @brief Vulkan debug report callback function (active if APP_USE_VULKAN_DEBUG_REPORT is defined).
     * @param flags Debug report flags.
     * @param objectType Type of the object causing the report.
     * @param object Handle of the object causing the report.
     * @param location Location identifier.
     * @param messageCode Message code.
     * @param pLayerPrefix Layer prefix string.
     * @param pMessage The debug message.
     * @param pUserData User data pointer.
     * @return VK_FALSE (as per Vulkan spec for this callback type).
     */
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *pLayerPrefix, const char *pMessage, void *pUserData);

    /**
     * @brief Checks if a Vulkan instance or device extension is available.
     * @param properties List of available extension properties.
     * @param extension The name of the extension to check for.
     * @return True if the extension is available, false otherwise.
     */
    bool IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties, const char *extension);

    /**
     * @brief Sets up the core Vulkan instance, physical device, logical device, and descriptor pool.
     * @param instance_extensions Required Vulkan instance extensions (e.g., from GLFW).
     */
    void SetupVulkan(ImVector<const char *> instance_extensions);

    /**
     * @brief Sets up Vulkan resources specific to a window surface (swapchain, format, present mode).
     * Uses ImGui_ImplVulkanH helper functions.
     * @param wd Pointer to the ImGui Vulkan helper window data structure.
     * @param surface The Vulkan surface associated with the window.
     * @param width The width of the framebuffer.
     * @param height The height of the framebuffer.
     */
    void SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width, int height);

    /**
     * @brief Cleans up core Vulkan resources (device, instance, descriptor pool, debug report).
     */
    void CleanupVulkan();

    /**
     * @brief Cleans up Vulkan resources associated with the main window (swapchain, etc.)
     * Uses ImGui_ImplVulkanH helper functions.
     */
    void CleanupVulkanWindow();

    /**
      * @brief Finds a suitable memory type index on the physical device.
      * @param typeFilter Bitmask of allowed memory types.
      * @param properties Required memory property flags.
      * @return The index of a suitable memory type.
      * @throws std::runtime_error if no suitable type is found.
      */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

     /**
      * @brief Helper function to create a Vulkan buffer and allocate/bind memory for it.
      * @param size The size of the buffer in bytes.
      * @param usage Buffer usage flags (e.g., vertex buffer, index buffer).
      * @param properties Required memory property flags (e.g., host visible, device local).
      * @param buffer Output handle for the created buffer.
      * @param bufferMemory Output handle for the allocated device memory.
      * @throws std::runtime_error on failure to create buffer or allocate/bind memory.
      */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);


    /**
     * @brief Gets the current extent (width and height) of the window.
     * @return VkExtent2D containing the width and height.
     */
    VkExtent2D getExtent();

    /**
     * @brief Loads the vertexbuffer into the model
     */
    void loadModels();

    /**
     * Handles wdjonkllllll
     */
    void fbResizeCallback(GLFWwindow* window, uint32_t width, uint32_t height);

    /**
     * @brief Initializes the GLFW window and sets up the Vulkan context, swapchain, pipeline, and ImGui.
     * @param Swidth Initial width of the window.
     * @param Sheight Initial height of the window.
     * @return 0 on success, 1 on failure.
     */
    int initialize(int Swidth, int Sheight);

    /**
     * @brief Cleans up all resources created by the Window module (pipeline, swapchain, ImGui, Vulkan, GLFW).
     */
    void cleanup();
}
#endif