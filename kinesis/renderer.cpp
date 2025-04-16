#include "renderer.h"
#include "window.h" // Include Window header for extent and device access
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cerr
#include <array>     // For std::array

namespace Kinesis::Renderer
{
    // Definition for extern variables
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t currentImageIndex = 0;
    int currentFrameIndex = 0;
    bool isFrameStarted = false;
    std::unique_ptr<Kinesis::SwapChain> SwapChain = nullptr; // Initialize to nullptr
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkExtent2D getExtent() {
        // It's generally better to get the extent from the window during recreation
        // but the swapchain holds the definitive extent for rendering.
        // If swapchain exists, use its extent, otherwise fallback to window.
        // However, during recreation, we NEED the window extent.
        // Let's keep using the window extent here for consistency during recreation.
        return {Kinesis::Window::width, Kinesis::Window::height};
    }

     void createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = g_QueueFamily; // Use the global queue family index
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(g_Device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }


    void freeCommandBuffers() {
         if (!commandBuffers.empty()) {
            vkFreeCommandBuffers(
                g_Device,
                commandPool, // Free from the renderer's command pool
                static_cast<uint32_t>(commandBuffers.size()),
                commandBuffers.data());
            commandBuffers.clear();
        }
    }


    void createCommandBuffers()
    {
        // Ensure command buffers are freed before resizing
        freeCommandBuffers();

        commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT); // Use the constant from SwapChain class

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool; // Allocate from the renderer's command pool
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(g_Device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recreateSwapChain() {
        // Get current window extent
        auto extent = getExtent();
        // Wait while window is minimized
        while (extent.width == 0 || extent.height == 0) {
          extent = getExtent();
          glfwWaitEvents(); // Process window events
        }
        // Wait for the device to be idle before recreating resources
        vkDeviceWaitIdle(g_Device);

        // If the swapchain doesn't exist, create it.
        if (SwapChain == nullptr) {
            std::cout << "iris" << std::endl;
          SwapChain = std::make_unique<Kinesis::SwapChain>(extent);
        } else {
          // If it exists, create a new one passing the old one for resource reuse
          std::shared_ptr<Kinesis::SwapChain> oldSwapChain = std::move(SwapChain); // Move ownership
          SwapChain = std::make_unique<Kinesis::SwapChain>(extent, oldSwapChain);

          // Check if image formats are compatible (optional but recommended)
          if (!oldSwapChain->compareSwapFormats(*SwapChain.get())) {
            // Handle incompatible format change if necessary (e.g., recreate pipelines)
            throw std::runtime_error("Swap chain image(or depth) format has changed! Pipeline recreation might be needed.");
          }
        }
         // Note: If pipelines are format-dependent, they might need recreation here.
         // Currently assuming pipelines are compatible or handled elsewhere (e.g., RenderSystem recreation)
         // Recreate command buffers as they depend on framebuffers which depend on the swapchain
         createCommandBuffers();
      }

    VkCommandBuffer beginFrame()
    {
        assert(!isFrameStarted && "Double frame start!");
        assert(SwapChain != nullptr && "Swapchain not initialized before beginning frame!");

        // Acquire an image from the swap chain
        auto result = SwapChain->acquireNextImage(&currentImageIndex);

        // Handle swapchain becoming outdated or suboptimal
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return nullptr; // Indicate frame cannot be started
        }
        // Handle other acquisition errors
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swapchain image!");
        }

        isFrameStarted = true;

        // Get the command buffer for the current frame in flight
        auto commandBuffer = currCommandBuffer();

        // Begin recording the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT can be useful if commands change every frame
        // beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer for frame index " + std::to_string(currentFrameIndex));
        }
        return commandBuffer;
    }

    void endFrame()
    {
        assert(isFrameStarted && "No frame to end!");
        assert(SwapChain != nullptr && "Swapchain not initialized before ending frame!");

        auto commandBuffer = currCommandBuffer();

        // End command buffer recording
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }

        // Submit the command buffer to the graphics queue
        auto result = SwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

        // Handle swapchain presentation issues (outdated, suboptimal, or window resized)
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || Kinesis::Window::fbResized)
        {
            Kinesis::Window::fbResized = false; // Reset the resize flag
            try{
                recreateSwapChain();
            }catch (const std::exception& e) {
                // Log the error, but potentially continue if possible, or rethrow/exit
                std::cerr << "Failed to recreate swapchain during endFrame: " << e.what() << std::endl;
                // Depending on the error, you might want to rethrow or handle gracefully
                // throw; // Re-throw if it's critical
            }
        } else if (result != VK_SUCCESS) // Handle other presentation errors
        {
            throw std::runtime_error("failed to present swapchain image!");
        }

        isFrameStarted = false;

        // Advance to the next frame index, wrapping around based on MAX_FRAMES_IN_FLIGHT
        currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
    }

    void beginSwapChainRenderPass(VkCommandBuffer cmdBuffer)
    {
        assert(isFrameStarted && "No frame to start render pass!");
        assert(cmdBuffer == currCommandBuffer() && "Mismatching command buffers for render pass start!");
        assert(SwapChain != nullptr && "Swapchain not initialized before beginning render pass!");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = SwapChain->getRenderPass(); // Get render pass from SwapChain object
        renderPassInfo.framebuffer = SwapChain->getFrameBuffer(currentImageIndex); // Get framebuffer for the acquired image index

        // Define the render area
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = SwapChain->getSwapChainExtent(); // Use swapchain's extent

        // Define clear values for attachments (color and depth)
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f}; // Clear color
        clearValues[1].depthStencil = {1.0f, 0}; // Clear depth to 1.0, stencil to 0
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Begin the render pass
        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE); // Use INLINE for primary command buffer

        // Set dynamic viewport state
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(SwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(SwapChain->getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        // Set dynamic scissor state
        VkRect2D scissor{{0, 0}, SwapChain->getSwapChainExtent()};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    }

    void endSwapChainRenderPass(VkCommandBuffer cmdBuffer)
    {
        assert(isFrameStarted && "No frame to end render pass!");
        assert(cmdBuffer == currCommandBuffer() && "Mismatching command buffers for render pass end!");
        // End the render pass
        vkCmdEndRenderPass(cmdBuffer);
    }

    void Initialize()
    {
        createCommandPool(); // Create the renderer's command pool first
        std::cout << "pewter" << std::endl;
        recreateSwapChain(); // Create initial swapchain and dependent resources (like command buffers)
        // Note: createCommandBuffers is now called inside recreateSwapChain
    }

    void Cleanup()
    {
         // Wait for device idle before cleanup
        vkDeviceWaitIdle(g_Device);

        // Free command buffers first
        freeCommandBuffers();

        // Destroy the swapchain object (unique_ptr handles deletion)
        SwapChain.reset();

        // Destroy the command pool
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(g_Device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
    }
}