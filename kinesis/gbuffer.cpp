// kinesis/gbuffer.cpp 
#include "gbuffer.h"
#include "window.h"
#include "swapchain.h"
#include <stdexcept>
#include <array>
#include <cassert>

// Make sure to include kinesis.h if g_Device is used here but not via window.h
#include "kinesis.h" 

namespace Kinesis::GBuffer { // Use the inner namespace

    // Define and initialize the variables declared extern in the header
    VkFramebuffer frameBuffer = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    PositionAttachment positionAttachment = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    NormalAttachment normalAttachment = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    AlbedoAttachment albedoAttachment = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    PropertiesAttachment propertiesAttachment = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    DepthAttachment depthAttachment = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    VkSampler sampler = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0}; // Initialize extent

    // Helper to create image attachments (no changes needed here, but ensure it uses g_Device correctly)
    void createImageAttachment(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* memory, VkImageView* view) {
         // ... (Keep existing implementation)
         VkImageCreateInfo imageInfo{};
         imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
         imageInfo.imageType = VK_IMAGE_TYPE_2D;
         imageInfo.format = format;
         imageInfo.extent = { width, height, 1 };
         imageInfo.mipLevels = 1;
         imageInfo.arrayLayers = 1;
         imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
         imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
         imageInfo.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT; // Ensure SAMPLED_BIT for later reads
         imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

         if (vkCreateImage(g_Device, &imageInfo, nullptr, image) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create G-Buffer image!");
         }

         VkMemoryRequirements memRequirements;
         vkGetImageMemoryRequirements(g_Device, *image, &memRequirements);

         VkMemoryAllocateInfo allocInfo{};
         allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
         allocInfo.allocationSize = memRequirements.size;
         // Use the global findMemoryType helper (ensure it's accessible, e.g., from window.h or kinesis.h)
         // Assuming Kinesis::Window::findMemoryType exists and works
         allocInfo.memoryTypeIndex = Kinesis::Window::findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); 

         if (vkAllocateMemory(g_Device, &allocInfo, nullptr, memory) != VK_SUCCESS) {
             throw std::runtime_error("Failed to allocate G-Buffer image memory!");
         }
         vkBindImageMemory(g_Device, *image, *memory, 0);

         VkImageViewCreateInfo viewInfo{};
         viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
         viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
         viewInfo.format = format;
         viewInfo.subresourceRange = {};
         // Determine aspect mask based on usage
         if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
             viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
         } else {
             viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         }
         viewInfo.subresourceRange.baseMipLevel = 0; // Added baseMipLevel
         viewInfo.subresourceRange.levelCount = 1;
         viewInfo.subresourceRange.baseArrayLayer = 0; // Added baseArrayLayer
         viewInfo.subresourceRange.layerCount = 1;
         viewInfo.image = *image;

         if (vkCreateImageView(g_Device, &viewInfo, nullptr, view) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create G-Buffer image view!");
         }
    }
    
    // ...(rest of gbuffer.cpp implementation remains the same)...
    void createRenderPass(VkFormat depthFormat) {
        // <<< INCREASE ARRAY SIZE FOR NEW ATTACHMENT >>>
        std::array<VkAttachmentDescription, 5> attachments = {}; // Position, Normal, Albedo, Properties, Depth

        // Position (World Space) - Attachment 0
        attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT; // High precision for position
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Need to store it
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Ready for sampling

        // Normal (World Space) + Roughness - Attachment 1
        attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT; // High precision for normal, float for roughness in alpha?
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Albedo (Base Color) - Attachment 2
        attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM; // Standard color format
        attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Properties (Metallic/IOR/Type/Etc.) - Attachment 3 <<< NEW >>>
        attachments[3].format = VK_FORMAT_R8G8B8A8_UNORM; // Example format, adjust based on packing
        attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth - Attachment 4 (was 3)
        attachments[4].format = depthFormat;
        attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't need depth after G-buffer pass usually
        attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // <<< INCREASE ARRAY SIZE >>>
        std::array<VkAttachmentReference, 4> colorReferences = {};
        colorReferences[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Position
        colorReferences[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Normal
        colorReferences[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Albedo
        colorReferences[3] = { 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Properties <<< NEW >>>

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 4; // <<< UPDATED INDEX >>>
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size()); // <<< UPDATED SIZE >>>
        subpass.pColorAttachments = colorReferences.data();
        subpass.pDepthStencilAttachment = &depthReference;

        // Subpass Dependencies (Ensure G-Buffer writes complete before RT reads)
        // Dependency 1: External -> GBuffer Pass
        VkSubpassDependency dependency1 = {};
        dependency1.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency1.dstSubpass = 0; // Our G-Buffer subpass
        // Wait for previous color writes (or start) before we write color/depth
        dependency1.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency1.srcAccessMask = 0; // No access needed from previous stage for clear
        dependency1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Dependency 2: GBuffer Pass -> External (Next Pass, e.g., Ray Tracing or Composition)
        VkSubpassDependency dependency2 = {};
        dependency2.srcSubpass = 0; // Our G-Buffer subpass
        dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
         // Wait for G-Buffer writes to finish
        dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        // Before the next pass starts reading (e.g., RayGen shader or Compute shader)
        dependency2.dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
         dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
         // Make written data available for reading as shader inputs
         dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
         dependency2.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


        std::array<VkSubpassDependency, 2> dependencies = {dependency1, dependency2};


        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); // <<< UPDATED SIZE >>>
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size()); // <<< UPDATED SIZE >>>
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(g_Device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create G-Buffer render pass!");
        }
    }

    void createFramebuffer(VkFormat depthFormat) {
         assert(renderPass != VK_NULL_HANDLE);

         // <<< INCREASE ARRAY SIZE AND ADD NEW VIEW >>>
         std::array<VkImageView, 5> attachmentsViews = { // Renamed to avoid confusion
             positionAttachment.view, // Use the correct member name
             normalAttachment.view,
             albedoAttachment.view,
             propertiesAttachment.view, // <<< NEW >>>
             depthAttachment.view
         };

         VkFramebufferCreateInfo framebufferInfo = {};
         framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         framebufferInfo.renderPass = renderPass;
         framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentsViews.size()); // <<< UPDATED SIZE >>>
         framebufferInfo.pAttachments = attachmentsViews.data();
         framebufferInfo.width = extent.width;
         framebufferInfo.height = extent.height;
         framebufferInfo.layers = 1;

         if (vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &frameBuffer) != VK_SUCCESS) {
             throw std::runtime_error("failed to create G-Buffer framebuffer!");
         }
    }

     void createSampler() {
         VkSamplerCreateInfo samplerInfo{};
         samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
         samplerInfo.magFilter = VK_FILTER_NEAREST; // Use NEAREST for G-Buffer lookup
         samplerInfo.minFilter = VK_FILTER_NEAREST;
         samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         samplerInfo.anisotropyEnable = VK_FALSE;
         samplerInfo.maxAnisotropy = 1.0f;
         samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; // Use float border color
         samplerInfo.unnormalizedCoordinates = VK_FALSE; // Use normalized coordinates [0, 1]
         samplerInfo.compareEnable = VK_FALSE;
         samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
         samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
         samplerInfo.mipLodBias = 0.0f;
         samplerInfo.minLod = 0.0f;
         samplerInfo.maxLod = 0.0f; // No mipmaps for G-buffer

         if (vkCreateSampler(g_Device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create G-Buffer sampler!");
         }
     }

    void setup(uint32_t width, uint32_t height, VkFormat depthFormat) {
        cleanup(); // Clean up previous resources if any

        extent = {width, height};

        // --- Create image attachments ---
        // Position
        createImageAttachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              &positionAttachment.image, &positionAttachment.memory, &positionAttachment.view);
        // Normal + Roughness
        createImageAttachment(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              &normalAttachment.image, &normalAttachment.memory, &normalAttachment.view);
        // Albedo
        createImageAttachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              &albedoAttachment.image, &albedoAttachment.memory, &albedoAttachment.view);
        // Properties <<< NEW >>> Use a suitable format
        createImageAttachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              &propertiesAttachment.image, &propertiesAttachment.memory, &propertiesAttachment.view);

         // Depth attachment
        createImageAttachment(width, height, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             &depthAttachment.image, &depthAttachment.memory, &depthAttachment.view);


        // Create render pass, framebuffer, and sampler
        createRenderPass(depthFormat);
        createFramebuffer(depthFormat);
        createSampler();
    }

    void cleanup() {
        // Destroy sampler
        if (sampler != VK_NULL_HANDLE) {
            vkDestroySampler(g_Device, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }

        // Destroy framebuffer
        if (frameBuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(g_Device, frameBuffer, nullptr);
            frameBuffer = VK_NULL_HANDLE;
        }

        // Destroy render pass
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(g_Device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        // Destroy attachments
        auto destroyAttachment = [&](VkImageView& view, VkImage& image, VkDeviceMemory& memory) {
             if (view != VK_NULL_HANDLE) vkDestroyImageView(g_Device, view, nullptr);
             if (image != VK_NULL_HANDLE) vkDestroyImage(g_Device, image, nullptr);
             if (memory != VK_NULL_HANDLE) vkFreeMemory(g_Device, memory, nullptr);
             view = VK_NULL_HANDLE; image = VK_NULL_HANDLE; memory = VK_NULL_HANDLE;
        };
        destroyAttachment(positionAttachment.view, positionAttachment.image, positionAttachment.memory);
        destroyAttachment(normalAttachment.view, normalAttachment.image, normalAttachment.memory);
        destroyAttachment(albedoAttachment.view, albedoAttachment.image, albedoAttachment.memory);
        destroyAttachment(propertiesAttachment.view, propertiesAttachment.image, propertiesAttachment.memory); // <<< NEW >>>
        destroyAttachment(depthAttachment.view, depthAttachment.image, depthAttachment.memory);
    }
} // namespace Kinesis::GBuffer