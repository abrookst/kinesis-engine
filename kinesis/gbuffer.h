// kinesis/gbuffer.h 
#ifndef GBUFFER_H
#define GBUFFER_H

#include "kinesis.h" // For Vulkan types
#include <vector>

namespace Kinesis {

    namespace GBuffer {
        // Use extern for declarations in the header
        extern VkFramebuffer frameBuffer;
        extern VkRenderPass renderPass;

        struct PositionAttachment { // Renamed struct for clarity
            VkImage image;
            VkDeviceMemory memory;
            VkImageView view;
        };
        extern PositionAttachment positionAttachment;

        struct NormalAttachment { // Renamed struct
            VkImage image;
            VkDeviceMemory memory;
            VkImageView view;
        };
        extern NormalAttachment normalAttachment;

        struct AlbedoAttachment { // Renamed struct
            VkImage image;
            VkDeviceMemory memory;
            VkImageView view;
        };
        extern AlbedoAttachment albedoAttachment;

        struct PropertiesAttachment { // Renamed struct
            VkImage image;
            VkDeviceMemory memory;
            VkImageView view;
        };
        extern PropertiesAttachment propertiesAttachment;


        struct DepthAttachment { // Kept struct name
             VkImage image;
             VkDeviceMemory memory;
             VkImageView view;
        };
        extern DepthAttachment depthAttachment;

        extern VkSampler sampler;
        extern VkExtent2D extent;

        // Keep function declarations
        void setup(uint32_t width, uint32_t height, VkFormat depthFormat);
        void cleanup();
        void createImageAttachment(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* memory, VkImageView* view);
        void createRenderPass(VkFormat depthFormat);
        void createFramebuffer(VkFormat depthFormat);
        void createSampler();
    }

}

#endif