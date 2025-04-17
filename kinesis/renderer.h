#ifndef RENDERER_H
#define RENDERER_H

#include <Vector>
#include <cassert>
#include <memory> // Required for std::unique_ptr

#include "kinesis.h"
#include "swapchain.h" // Include SwapChain header

namespace Kinesis::Renderer {
    extern std::vector<VkCommandBuffer> commandBuffers;
    extern uint32_t currentImageIndex;
    extern int currentFrameIndex;
    extern bool isFrameStarted;
    extern std::unique_ptr<Kinesis::SwapChain> SwapChain; // Use the class directly
    extern VkCommandPool commandPool; // Add a command pool managed by the renderer

    inline VkCommandBuffer currCommandBuffer() {
        assert(isFrameStarted && "cannot get command buffer when there is no current frame.");
        // Ensure currentFrameIndex is within bounds of allocated command buffers
        assert(currentFrameIndex >= 0 && currentFrameIndex < commandBuffers.size() && "currentFrameIndex out of bounds");
        return commandBuffers[currentFrameIndex];
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer cmdBuffer);
    void endSwapChainRenderPass(VkCommandBuffer cmdBuffer);

    float getAspectRatio();

    /**
     * @brief Rerenders the swapchain given a window resizing.
     */
    void recreateSwapChain();


    /**
     * @brief Creates and records command buffers for rendering.
     */
    void createCommandBuffers();

     /**
     * @brief Creates the command pool for the renderer.
     */
    void createCommandPool();

     /**
      * @brief Frees command buffers allocated from the renderer's pool.
      */
     void freeCommandBuffers(); // Added declaration

    /**
     * @brief Gets the current extent (width and height) of the window.
     * @return VkExtent2D containing the width and height.
     */
    VkExtent2D getExtent();

    void Initialize();

    void Cleanup();
}

#endif