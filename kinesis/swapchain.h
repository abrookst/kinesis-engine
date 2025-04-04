#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "kinesis.h" // Include necessary headers (Vulkan etc.)

// std lib headers
#include <string>
#include <vector>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace Kinesis::Swapchain
{
    // --- Public Interface ---

    /**
     * @brief Maximum number of frames that can be processed concurrently.
     */
    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    /**
     * @brief Structure containing details about swap chain support.
     */
    struct SupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    /**
     * @brief Initializes the swapchain and related resources.
     * @param extent The initial window extent.
     */
    void initialize(VkExtent2D extent);

    /**
     * @brief Cleans up swapchain resources.
     */
    void cleanup();

    /**
     * @brief Gets the framebuffer for a specific swap chain image index.
     * @param index The index of the swap chain image.
     * @return Handle to the framebuffer.
     */
    VkFramebuffer getFrameBuffer(int index);

    /**
     * @brief Gets the render pass associated with the swap chain.
     * @return Handle to the render pass.
     */
    VkRenderPass getRenderPass();

    /**
     * @brief Gets the image view for a specific swap chain image index.
     * @param index The index of the swap chain image.
     * @return Handle to the image view.
     */
    VkImageView getImageView(int index);

    /**
     * @brief Gets the number of images in the swap chain.
     * @return The image count.
     */
    size_t imageCount();

    /**
     * @brief Gets the format of the swap chain images.
     * @return The image format.
     */
    VkFormat getSwapChainImageFormat();

    /**
     * @brief Gets the extent (resolution) of the swap chain images.
     * @return The swap chain extent.
     */
    VkExtent2D getSwapChainExtent();

    /**
     * @brief Gets the width of the swap chain images.
     * @return The width.
     */
    uint32_t width();

    /**
     * @brief Gets the height of the swap chain images.
     * @return The height.
     */
    uint32_t height();

    /**
     * @brief Calculates the aspect ratio of the swap chain extent.
     * @return The aspect ratio.
     */
    float extentAspectRatio();

    /**
     * @brief Finds a suitable depth format supported by the physical device.
     * @return The supported depth format.
     */
    VkFormat findDepthFormat();

    /**
     * @brief Acquires the next available image from the swap chain.
     * @param imageIndex Pointer to store the index of the acquired image.
     * @return Vulkan result code.
     */
    VkResult acquireNextImage(uint32_t *imageIndex);

    /**
     * @brief Submits command buffers for rendering and presents the image.
     * @param buffers Pointer to the command buffer(s) to submit.
     * @param imageIndex Pointer to the index of the image to present.
     * @return Vulkan result code.
     */
    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

     /**
     * @brief Helper function to create a VkImage with associated memory.
     * @param imageInfo Image creation info structure.
     * @param properties Desired memory properties.
     * @param image Output handle for the created image.
     * @param imageMemory Output handle for the allocated image memory.
     */
    void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory);

    /**
     * @brief Queries swap chain support details for a given physical device.
     * @param device The physical device to query.
     * @return Structure containing swap chain support details.
     */
    SupportDetails querySwapChainSupport(VkPhysicalDevice device);

    /**
     * @brief Gets swap chain support details for the default physical device.
     * @return Structure containing swap chain support details.
     */
    SupportDetails getSwapChainSupport();

}

#endif