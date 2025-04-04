#include "swapchain.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <algorithm> // Required for std::max/min

// Make sure kinesis.h provides necessary definitions (g_Device, etc.)
// If not already included via swapchain.h -> kinesis.h, include it here.
// #include "kinesis.h"

namespace Kinesis::Swapchain
{
    // --- Static Variables (Former Private Members) ---
    namespace { // Use anonymous namespace for internal linkage
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        VkExtent2D windowExtent; // Keep track of the window extent passed during init

        VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;

        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        std::vector<VkImage> depthImages;
        std::vector<VkDeviceMemory> depthImageMemorys;
        std::vector<VkImageView> depthImageViews;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;
    } // anonymous namespace

    // --- Helper Function Declarations (Internal Implementation Details) ---
    namespace { // Use anonymous namespace for internal linkage
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

        void createSwapChainInternal(); // Renamed from createSwapChain to avoid conflict
        void createImageViewsInternal(); // Renamed
        void createRenderPassInternal(); // Renamed
        void createDepthResourcesInternal(); // Renamed
        void createFramebuffersInternal(); // Renamed
        void createSyncObjectsInternal(); // Renamed
    } // anonymous namespace

    // --- Public Function Definitions ---

    void initialize(VkExtent2D extent)
    {
        // Ensure required global handles are initialized before proceeding
        if (g_Device == VK_NULL_HANDLE || g_PhysicalDevice == VK_NULL_HANDLE || g_MainWindowData.Surface == VK_NULL_HANDLE) {
            throw std::runtime_error("Required Vulkan handles not initialized before Swapchain::initialize!");
        }
        windowExtent = extent; // Store the initial extent

        // Call internal creation functions in the correct order
        createSwapChainInternal();
        createImageViewsInternal();
        createRenderPassInternal();
        createDepthResourcesInternal();
        createFramebuffersInternal();
        createSyncObjectsInternal();
    }

    void cleanup()
    {
        // Ensure g_Device is valid before cleanup
        if (g_Device == VK_NULL_HANDLE) {
             // Optionally log a warning or just return if already cleaned up
             // std::cerr << "Warning: Attempting Swapchain::cleanup with null g_Device." << std::endl;
             return;
        }

        // Cleanup synchronization objects first
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
             if (renderFinishedSemaphores.size() > i && renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(g_Device, renderFinishedSemaphores[i], nullptr);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE; // Mark as destroyed
             }
             if (imageAvailableSemaphores.size() > i && imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(g_Device, imageAvailableSemaphores[i], nullptr);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
             }
             if (inFlightFences.size() > i && inFlightFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(g_Device, inFlightFences[i], nullptr);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
        }
        renderFinishedSemaphores.clear();
        imageAvailableSemaphores.clear();
        inFlightFences.clear();
        imagesInFlight.clear(); // Fences are owned by inFlightFences, just clear the vector


        // Cleanup framebuffers
        for (auto framebuffer : swapChainFramebuffers)
        {
             if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(g_Device, framebuffer, nullptr);
            }
        }
        swapChainFramebuffers.clear();

        // Cleanup render pass
         if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(g_Device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Cleanup depth resources
        for (size_t i = 0; i < depthImages.size(); i++)
        {
             if (depthImageViews.size() > i && depthImageViews[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(g_Device, depthImageViews[i], nullptr);
            }
             if (depthImages.size() > i && depthImages[i] != VK_NULL_HANDLE) {
                vkDestroyImage(g_Device, depthImages[i], nullptr);
            }
             if (depthImageMemorys.size() > i && depthImageMemorys[i] != VK_NULL_HANDLE) {
                vkFreeMemory(g_Device, depthImageMemorys[i], nullptr);
            }
        }
        depthImageViews.clear();
        depthImages.clear();
        depthImageMemorys.clear();

        // Cleanup image views (associated with swapChainImages)
        for (auto imageView : swapChainImageViews)
        {
             if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(g_Device, imageView, nullptr);
            }
        }
        swapChainImageViews.clear();

        // Cleanup swap chain itself (this also destroys swapChainImages)
         if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(g_Device, swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
        }

        // Clear image handles vector (images are owned by swapchain)
        swapChainImages.clear();

        // Reset other state if necessary
        currentFrame = 0;
    }

    VkFramebuffer getFrameBuffer(int index) {
        if (index >= 0 && index < swapChainFramebuffers.size()) {
            return swapChainFramebuffers[index];
        }
        // Consider logging an error or throwing an exception for invalid index
        // Returning VK_NULL_HANDLE might hide errors elsewhere.
         throw std::out_of_range("Invalid index for getFrameBuffer");
        // return VK_NULL_HANDLE;
     }

    VkRenderPass getRenderPass() { return renderPass; }

    VkImageView getImageView(int index) {
        if (index >= 0 && index < swapChainImageViews.size()) {
           return swapChainImageViews[index];
        }
         throw std::out_of_range("Invalid index for getImageView");
        // return VK_NULL_HANDLE;
    }

    size_t imageCount() { return swapChainImages.size(); }

    VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }

    VkExtent2D getSwapChainExtent() { return swapChainExtent; }

    uint32_t width() { return swapChainExtent.width; }

    uint32_t height() { return swapChainExtent.height; }

    float extentAspectRatio()
    {
        if (swapChainExtent.height == 0) return 0.0f; // Avoid division by zero
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    }

    VkFormat findDepthFormat()
    {
        if (g_PhysicalDevice == VK_NULL_HANDLE) {
             throw std::runtime_error("g_PhysicalDevice is null in findDepthFormat!");
        }
        return findSupportedFormat( // Calls internal helper
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkResult acquireNextImage(uint32_t *imageIndex)
    {
        if (g_Device == VK_NULL_HANDLE) return VK_ERROR_DEVICE_LOST; // Or throw
        if (swapChain == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED; // Or throw

        // Ensure synchronization objects are ready
        if (currentFrame >= MAX_FRAMES_IN_FLIGHT || currentFrame >= inFlightFences.size() || currentFrame >= imageAvailableSemaphores.size() || inFlightFences[currentFrame] == VK_NULL_HANDLE || imageAvailableSemaphores[currentFrame] == VK_NULL_HANDLE ) {
            throw std::runtime_error("Synchronization objects not ready in acquireNextImage!");
        }

        // Wait for the fence of the frame we are about to render
        VkResult waitResult = vkWaitForFences(
            g_Device,
            1,
            &inFlightFences[currentFrame],
            VK_TRUE, // Wait for all fences (only one here)
            UINT64_MAX); // Wait indefinitely
        if (waitResult != VK_SUCCESS) {
            // Handle timeout or error
             return waitResult;
        }


        VkResult result = vkAcquireNextImageKHR(
            g_Device,
            swapChain,
            UINT64_MAX, // Timeout (indefinite)
            imageAvailableSemaphores[currentFrame], // Semaphore to signal
            VK_NULL_HANDLE,                         // Fence to signal (optional)
            imageIndex); // Output image index

        // Note: result could be VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR,
        // indicating swapchain needs recreation. This is not handled here yet.

        return result;
    }

    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex)
    {
        if (g_Device == VK_NULL_HANDLE || g_Queue == VK_NULL_HANDLE) return VK_ERROR_DEVICE_LOST; // Or throw
        if (swapChain == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED; // Or throw

        // Bounds check imageIndex
        if (*imageIndex >= imagesInFlight.size() || *imageIndex >= imageCount()) {
             throw std::out_of_range("Invalid image index received in submitCommandBuffers!");
        }
        // Check currentFrame validity
        if (currentFrame >= MAX_FRAMES_IN_FLIGHT || currentFrame >= inFlightFences.size() || currentFrame >= imageAvailableSemaphores.size() || currentFrame >= renderFinishedSemaphores.size() || inFlightFences[currentFrame] == VK_NULL_HANDLE || imageAvailableSemaphores[currentFrame] == VK_NULL_HANDLE || renderFinishedSemaphores[currentFrame] == VK_NULL_HANDLE) {
             throw std::runtime_error("Synchronization objects not ready in submitCommandBuffers!");
        }


        // Wait for the fence associated *with the image index* if it's in use by a previous frame
        if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE)
        {
            VkResult waitResult = vkWaitForFences(g_Device, 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
             if (waitResult != VK_SUCCESS) return waitResult; // Handle error
        }
        // Mark the image as now being in use by the *current frame's* fence
        imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // Wait at this stage
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1; // Assuming one command buffer per submission for now
        submitInfo.pCommandBuffers = buffers;

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]}; // Signal this when done
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // Reset the current frame's fence before submitting
        vkResetFences(g_Device, 1, &inFlightFences[currentFrame]);

        // Submit to the graphics queue
        VkResult submitResult = vkQueueSubmit(g_Queue, 1, &submitInfo, inFlightFences[currentFrame]); // Use current frame's fence
        if ( submitResult != VK_SUCCESS)
        {
             // Don't throw immediately, let caller handle VKResult
             // throw std::runtime_error("failed to submit draw command buffer!");
             return submitResult;
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; // Wait for rendering to finish

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = imageIndex;
        presentInfo.pResults = nullptr; // Optional

        // Present the image to the presentation queue
        VkResult presentResult = vkQueuePresentKHR(g_Queue, &presentInfo);

        // Advance to the next frame index
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        return presentResult; // Return the result of vkQueuePresentKHR
    }

    void createImageWithInfo(
        const VkImageCreateInfo &imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage &image,
        VkDeviceMemory &imageMemory) {
        if (g_Device == VK_NULL_HANDLE) {
            throw std::runtime_error("g_Device is null in createImageWithInfo!");
        }
      if (vkCreateImage(g_Device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
      }

      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(g_Device, image, &memRequirements);

      VkMemoryAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocInfo.allocationSize = memRequirements.size;
      allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties); // Uses internal helper

      if (vkAllocateMemory(g_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        // Clean up created image if memory allocation fails
         vkDestroyImage(g_Device, image, nullptr);
         image = VK_NULL_HANDLE;
        throw std::runtime_error("failed to allocate image memory!");
      }

      // Bind the memory to the image
      if (vkBindImageMemory(g_Device, image, imageMemory, 0) != VK_SUCCESS) {
         // Clean up created image and allocated memory
         vkDestroyImage(g_Device, image, nullptr);
         image = VK_NULL_HANDLE;
         vkFreeMemory(g_Device, imageMemory, nullptr);
         imageMemory = VK_NULL_HANDLE;
        throw std::runtime_error("failed to bind image memory!");
      }
    }

    SupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        if (device == VK_NULL_HANDLE) {
            throw std::runtime_error("Physical device is null in querySwapChainSupport!");
        }
        if (g_MainWindowData.Surface == VK_NULL_HANDLE) {
             throw std::runtime_error("g_MainWindowData.Surface is null in querySwapChainSupport!");
        }
        VkSurfaceKHR surface_ = g_MainWindowData.Surface;
        SupportDetails details;

        // Query capabilities
        VkResult capResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);
         if (capResult != VK_SUCCESS) {
             throw std::runtime_error("Failed to get surface capabilities!");
         }


        // Query formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        if (formatCount != 0) {
          details.formats.resize(formatCount);
          vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        } else {
             throw std::runtime_error("No surface formats found!");
        }

        // Query present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
          details.presentModes.resize(presentModeCount);
          vkGetPhysicalDeviceSurfacePresentModesKHR(
              device, surface_, &presentModeCount, details.presentModes.data());
        } else {
             throw std::runtime_error("No present modes found!");
        }
        return details;
      }

    SupportDetails getSwapChainSupport() {
         if (g_PhysicalDevice == VK_NULL_HANDLE) {
             throw std::runtime_error("g_PhysicalDevice is null in getSwapChainSupport!");
        }
        return querySwapChainSupport(g_PhysicalDevice); // Calls the function above
    }


    // --- Helper Function Definitions (Internal Implementation Details) ---
    namespace { // Use anonymous namespace for internal linkage

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
             if (g_PhysicalDevice == VK_NULL_HANDLE) {
                 throw std::runtime_error("g_PhysicalDevice is null in findMemoryType!");
            }
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &memProperties);
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
              // Check if the bit for this memory type is set in the filter
              // AND check if all the required property flags are set for this memory type
              if ((typeFilter & (1 << i)) &&
                  (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i; // Found a suitable type
              }
            }
            throw std::runtime_error("failed to find suitable memory type!");
          }

        VkFormat findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
            if (g_PhysicalDevice == VK_NULL_HANDLE) {
                 throw std::runtime_error("g_PhysicalDevice is null in findSupportedFormat!");
            }
          for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(g_PhysicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
              return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
              return format;
            }
          }
          throw std::runtime_error("failed to find supported format!");
        }

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR> &availableFormats){
                for (const auto &availableFormat : availableFormats) {
                    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                      return availableFormat; // Prefer BGRA8 sRGB non-linear
                    }
                  }
                  if (availableFormats.empty()) {
                       throw std::runtime_error("No surface formats available!");
                  }
                  // Fallback to the first available format if preferred not found
                  return availableFormats[0];
            }

        VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes){
                for (const auto &availablePresentMode : availablePresentModes) {
                    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                      std::cout << "Present mode: Mailbox" << std::endl;
                      return availablePresentMode; // Prefer Mailbox for low latency
                    }
                  }
                 // FIFO is guaranteed to be supported by Vulkan spec
                 std::cout << "Present mode: V-Sync (FIFO)" << std::endl;
                 return VK_PRESENT_MODE_FIFO_KHR;
            }


        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities){
            // If current extent is defined, use it (often required by windowing system)
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
              } else {
                // Otherwise, clamp the desired window extent to the min/max allowed
                VkExtent2D actualExtent = windowExtent; // Use extent passed to initialize
                actualExtent.width = std::max(
                    capabilities.minImageExtent.width,
                    std::min(capabilities.maxImageExtent.width, actualExtent.width));
                actualExtent.height = std::max(
                    capabilities.minImageExtent.height,
                    std::min(capabilities.maxImageExtent.height, actualExtent.height));

                return actualExtent;
              }
        }

        void createSwapChainInternal()
        {
            SupportDetails swapChainSupport = getSwapChainSupport();

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities); // Store chosen extent
            swapChainImageFormat = surfaceFormat.format; // Store chosen format

            // Determine image count
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // Request one more than minimum
            if (swapChainSupport.capabilities.maxImageCount > 0 && // MaxImageCount == 0 means no limit
                imageCount > swapChainSupport.capabilities.maxImageCount)
            {
                imageCount = swapChainSupport.capabilities.maxImageCount; // Clamp to max
            }
             // Ensure we have at least the minimum needed (e.g., for double/triple buffering)
             imageCount = std::max(imageCount, g_MinImageCount); // Use global constant


            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = g_MainWindowData.Surface; // From global data

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = swapChainImageFormat;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = swapChainExtent;
            createInfo.imageArrayLayers = 1; // Non-stereoscopic
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Use images as color attachments

            // Handle queue family indices if graphics and present queues differ (not shown here)
            // For simplicity, assume they are the same (VK_SHARING_MODE_EXCLUSIVE)
             uint32_t queueFamilyIndices[] = { g_QueueFamily }; // Assuming same queue family from kinesis.h global
             createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
             createInfo.queueFamilyIndexCount = 0;
             createInfo.pQueueFamilyIndices = nullptr;
            // If sharing needed:
            // createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // createInfo.queueFamilyIndexCount = 2; // If different graphics and present families
            // createInfo.pQueueFamilyIndices = queueFamilyIndices;


            createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Usually identity
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Assume opaque window
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE; // Allow clipping pixels outside the visible region

            createInfo.oldSwapchain = VK_NULL_HANDLE; // Not handling recreation here

            if (vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create swap chain!");
            }

            // Get the actual image handles created by the implementation
            vkGetSwapchainImagesKHR(g_Device, swapChain, &imageCount, nullptr); // Get actual count
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(g_Device, swapChain, &imageCount, swapChainImages.data());

            // Potentially update ImGui's VulkanH helper window data if used extensively
            // g_MainWindowData.ImageCount = imageCount;
        }

        void createImageViewsInternal()
        {
             size_t imgCount = swapChainImages.size();
            swapChainImageViews.resize(imgCount);
            for (size_t i = 0; i < imgCount; i++)
            {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = swapChainImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = swapChainImageFormat; // Use stored format
                viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Default channel mapping
                viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Color aspect
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;

                if (vkCreateImageView(g_Device, &viewInfo, nullptr, &swapChainImageViews[i]) !=
                    VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create texture image view!");
                }
            }
        }

        void createRenderPassInternal() {
            // Color Attachment (Swapchain Image)
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = getSwapChainImageFormat();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear when beginning pass
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store result for presentation
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about previous layout
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Transition to present layout

            // Depth Attachment
            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = findDepthFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear depth at start of pass
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Usually don't need to store depth
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Optimal layout for depth tests

            // Attachment References for the Subpass
            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0; // Index in the attachments array below
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Layout during subpass

            VkAttachmentReference depthAttachmentRef = {};
            depthAttachmentRef.attachment = 1; // Index in the attachments array below
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Layout during subpass

            // Subpass Description
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef; // Use the color attachment
            subpass.pDepthStencilAttachment = &depthAttachmentRef; // Use the depth attachment

            // Subpass Dependency (Handles layout transitions)
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // Implicit subpass before render pass
            dependency.dstSubpass = 0;                   // Our first (and only) subpass
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Stages where writes might occur before pass
            dependency.srcAccessMask = 0; // No access required before pass starts for undefined layout
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Stages where writes occur in our subpass
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; // Access needed in our subpass

            // Render Pass Creation
            std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(g_Device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create render pass!");
            }
        }


        void createDepthResourcesInternal()
        {
            VkFormat depthFormat = findDepthFormat();
            VkExtent2D extent = getSwapChainExtent();
            size_t imgCount = imageCount();

            depthImages.resize(imgCount);
            depthImageMemorys.resize(imgCount);
            depthImageViews.resize(imgCount);

            for (size_t i = 0; i < imgCount; i++)
            {
                VkImageCreateInfo imageInfo{};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.extent.width = extent.width;
                imageInfo.extent.height = extent.height;
                imageInfo.extent.depth = 1;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.format = depthFormat;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // Best for GPU access
                imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Initial layout doesn't matter
                imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; // Use as depth/stencil buffer
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA
                imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume same queue
                imageInfo.flags = 0;

                // Use the public helper from this namespace
                createImageWithInfo(
                    imageInfo,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // Allocate on dedicated GPU memory
                    depthImages[i],
                    depthImageMemorys[i]);

                // Create the image view for the depth image
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = depthImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = depthFormat;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // Specify depth aspect
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;

                if (vkCreateImageView(g_Device, &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create depth texture image view!");
                }
            }
        }

        void createFramebuffersInternal()
        {
             size_t imgCount = imageCount();
            swapChainFramebuffers.resize(imgCount);
            for (size_t i = 0; i < imgCount; i++)
            {
                // Ensure corresponding views exist
                if (i >= swapChainImageViews.size() || i >= depthImageViews.size() || swapChainImageViews[i] == VK_NULL_HANDLE || depthImageViews[i] == VK_NULL_HANDLE) {
                     throw std::runtime_error("Image views not available for framebuffer creation!");
                }
                std::array<VkImageView, 2> attachments = {
                    swapChainImageViews[i], // Color attachment
                    depthImageViews[i]      // Depth attachment
                };

                VkExtent2D extent = getSwapChainExtent();
                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = renderPass; // Use the created render pass
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = extent.width;
                framebufferInfo.height = extent.height;
                framebufferInfo.layers = 1; // Number of layers in image array

                if (vkCreateFramebuffer(
                        g_Device,
                        &framebufferInfo,
                        nullptr, // Allocator callbacks
                        &swapChainFramebuffers[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }
            // If ImGui_ImplVulkanH_Window struct's Framebuffers were used directly,
            // they might need updating here. Example:
            // g_MainWindowData.Framebuffer = swapChainFramebuffers.data(); // If it expects a pointer
             // g_MainWindowData.FramebufferCount = swapChainFramebuffers.size();
        }

        void createSyncObjectsInternal()
        {
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            // Resize imagesInFlight based on the actual number of swapchain images
            imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create fences already signaled

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                if (vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                    vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                    vkCreateFence(g_Device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                {
                    // Cleanup already created objects for this frame before throwing
                     if(i < imageAvailableSemaphores.size() && imageAvailableSemaphores[i] != VK_NULL_HANDLE) vkDestroySemaphore(g_Device, imageAvailableSemaphores[i], nullptr);
                     if(i < renderFinishedSemaphores.size() && renderFinishedSemaphores[i] != VK_NULL_HANDLE) vkDestroySemaphore(g_Device, renderFinishedSemaphores[i], nullptr);
                     if(i < inFlightFences.size() && inFlightFences[i] != VK_NULL_HANDLE) vkDestroyFence(g_Device, inFlightFences[i], nullptr);

                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
            }
        }

    } // anonymous namespace

} // namespace Kinesis::Swapchain