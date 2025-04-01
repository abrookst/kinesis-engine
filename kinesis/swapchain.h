#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "window.h"

// vulkan headers
#include <vulkan/vulkan.h>

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

namespace Kinesis
{
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
      };

    class Swapchain
    {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        Swapchain(VkExtent2D extent) : windowExtent{extent}
        {
            createSwapChain();
            createImageViews();
            createRenderPass();
            createDepthResources();
            createFramebuffers();
            createSyncObjects();
        };
        ~Swapchain()
        {
            for (auto imageView : swapChainImageViews)
            {
                vkDestroyImageView(g_Device, imageView, nullptr);
            }
            swapChainImageViews.clear();

            if (swapChain != nullptr)
            {
                vkDestroySwapchainKHR(g_Device, swapChain, nullptr);
                swapChain = nullptr;
            }

            for (int i = 0; i < depthImages.size(); i++)
            {
                vkDestroyImageView(g_Device, depthImageViews[i], nullptr);
                vkDestroyImage(g_Device, depthImages[i], nullptr);
                vkFreeMemory(g_Device, depthImageMemorys[i], nullptr);
            }

            for (auto framebuffer : swapChainFramebuffers)
            {
                vkDestroyFramebuffer(g_Device, framebuffer, nullptr);
            }

            vkDestroyRenderPass(g_Device, renderPass, nullptr);

            // cleanup synchronization objects
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroySemaphore(g_Device, renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(g_Device, imageAvailableSemaphores[i], nullptr);
                vkDestroyFence(g_Device, inFlightFences[i], nullptr);
            }
        };

        Swapchain(const Swapchain &) = delete;
        void operator=(const Swapchain &) = delete;

        VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
        VkRenderPass getRenderPass() { return renderPass; }
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }
        size_t imageCount() { return swapChainImages.size(); }
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }
        uint32_t width() { return swapChainExtent.width; }
        uint32_t height() { return swapChainExtent.height; }

        float extentAspectRatio()
        {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }
        VkFormat findDepthFormat(){
            return findSupportedFormat(
                {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        VkResult acquireNextImage(uint32_t *imageIndex)
        {
            vkWaitForFences(
                g_Device,
                1,
                &inFlightFences[currentFrame],
                VK_TRUE,
                std::numeric_limits<uint64_t>::max());

            VkResult result = vkAcquireNextImageKHR(
                g_Device,
                swapChain,
                std::numeric_limits<uint64_t>::max(),
                imageAvailableSemaphores[currentFrame], // must be a not signaled semaphore
                VK_NULL_HANDLE,
                imageIndex);

            return result;
        }
        VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex)
        {
            if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE)
            {
                vkWaitForFences(g_Device, 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
            }
            imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = buffers;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            vkResetFences(g_Device, 1, &inFlightFences[currentFrame]);
            if (vkQueueSubmit(g_Queue, 1, &submitInfo, inFlightFences[currentFrame]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = {swapChain};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;

            presentInfo.pImageIndices = imageIndex;
            // could make a family queue seperately, idk would be nice?
            auto result = vkQueuePresentKHR(g_Queue, &presentInfo);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

            return result;
        }

        void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory) {
          if (vkCreateImage(g_Device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
          }
        
          VkMemoryRequirements memRequirements;
          vkGetImageMemoryRequirements(g_Device, image, &memRequirements);
        
          VkMemoryAllocateInfo allocInfo{};
          allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
          allocInfo.allocationSize = memRequirements.size;
          allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        
          if (vkAllocateMemory(g_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
          }
        
          if (vkBindImageMemory(g_Device, image, imageMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("failed to bind image memory!");
          }
        }
    
        
    
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
            VkSurfaceKHR surface_ = g_MainWindowData.Surface;
            SwapChainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);
          
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
          
            if (formatCount != 0) {
              details.formats.resize(formatCount);
              vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
            }
          
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
          
            if (presentModeCount != 0) {
              details.presentModes.resize(presentModeCount);
              vkGetPhysicalDeviceSurfacePresentModesKHR(
                  device,
                  surface_,
                  &presentModeCount,
                  details.presentModes.data());
            }
            return details;
          }
    
        SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(g_PhysicalDevice); }

    private:
        void createSwapChain()
        {
            SwapChainSupportDetails swapChainSupport = getSwapChainSupport();

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
            if (swapChainSupport.capabilities.maxImageCount > 0 &&
                imageCount > swapChainSupport.capabilities.maxImageCount)
            {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = g_MainWindowData.Surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional

            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;

            createInfo.oldSwapchain = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create swap chain!");
            }

            // we only specified a minimum number of images in the swap chain, so the implementation is
            // allowed to create a swap chain with more. That's why we'll first query the final number of
            // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
            // retrieve the handles.
            vkGetSwapchainImagesKHR(g_Device, swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(g_Device, swapChain, &imageCount, swapChainImages.data());

            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }
        void createImageViews()
        {
            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++)
            {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = swapChainImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = swapChainImageFormat;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
        void createDepthResources()
        {
            VkFormat depthFormat = findDepthFormat();
            VkExtent2D swapChainExtent = getSwapChainExtent();

            depthImages.resize(imageCount());
            depthImageMemorys.resize(imageCount());
            depthImageViews.resize(imageCount());

            for (int i = 0; i < depthImages.size(); i++)
            {
                VkImageCreateInfo imageInfo{};
                imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageInfo.imageType = VK_IMAGE_TYPE_2D;
                imageInfo.extent.width = swapChainExtent.width;
                imageInfo.extent.height = swapChainExtent.height;
                imageInfo.extent.depth = 1;
                imageInfo.mipLevels = 1;
                imageInfo.arrayLayers = 1;
                imageInfo.format = depthFormat;
                imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                imageInfo.flags = 0;

                createImageWithInfo(
                    imageInfo,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    depthImages[i],
                    depthImageMemorys[i]);

                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = depthImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = depthFormat;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;

                if (vkCreateImageView(g_Device, &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create texture image view!");
                }
            }
        }
        void createRenderPass() {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = getSwapChainImageFormat();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = findDepthFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
            VkAttachmentReference depthAttachmentRef = {};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        
            // Add dependency to ensure ImGui rendering happens after main rendering
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
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
        void createFramebuffers()
        {
            swapChainFramebuffers.resize(imageCount());
            for (size_t i = 0; i < imageCount(); i++)
            {
                std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageViews[i]};

                VkExtent2D swapChainExtent = getSwapChainExtent();
                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = renderPass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = swapChainExtent.width;
                framebufferInfo.height = swapChainExtent.height;
                framebufferInfo.layers = 1;

                if (vkCreateFramebuffer(
                        g_Device,
                        &framebufferInfo,
                        nullptr,
                        &swapChainFramebuffers[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }
        }
        void createSyncObjects()
        {
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                if (vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
                        VK_SUCCESS ||
                    vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
                        VK_SUCCESS ||
                    vkCreateFence(g_Device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
            }
        }

        // Helper functions
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &memProperties);
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
              if ((typeFilter & (1 << i)) &&
                  (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
              }
            }
          
            throw std::runtime_error("failed to find suitable memory type!");
          }

        VkFormat findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
          for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(g_PhysicalDevice, format, &props);
        
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
              return format;
            } else if (
                tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
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
                      return availableFormat;
                    }
                  }
                
                  return availableFormats[0];
            }
        VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes){
                for (const auto &availablePresentMode : availablePresentModes) {
                    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                      std::cout << "Present mode: Mailbox" << std::endl;
                      return availablePresentMode;
                    }
                  }
                
                  // for (const auto &availablePresentMode : availablePresentModes) {
                  //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                  //     std::cout << "Present mode: Immediate" << std::endl;
                  //     return availablePresentMode;
                  //   }
                  // }
                
                  std::cout << "Present mode: V-Sync" << std::endl;
                  return VK_PRESENT_MODE_FIFO_KHR;
            }


        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities){
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
              } else {
                VkExtent2D actualExtent = windowExtent;
                actualExtent.width = std::max(
                    capabilities.minImageExtent.width,
                    std::min(capabilities.maxImageExtent.width, actualExtent.width));
                actualExtent.height = std::max(
                    capabilities.minImageExtent.height,
                    std::min(capabilities.maxImageExtent.height, actualExtent.height));
            
                return actualExtent;
              }
        }

        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        std::vector<VkFramebuffer> swapChainFramebuffers;
        VkRenderPass renderPass;

        std::vector<VkImage> depthImages;
        std::vector<VkDeviceMemory> depthImageMemorys;
        std::vector<VkImageView> depthImageViews;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

        VkExtent2D windowExtent;

        VkSwapchainKHR swapChain;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;
    };
}
#endif