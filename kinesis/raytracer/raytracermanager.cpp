// --- kinesis/RayTracingManager.cpp ---
#include "RayTracingManager.h"
#include "GUI.h"
#include <stdexcept>
#include <vector>
#include <array>
#include <iostream> // For std::cout

namespace Kinesis::RayTracingManager {

    // Define extern variables
    VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet rtDescriptorSet = VK_NULL_HANDLE;
    // Define other extern variables here...

    // --- Helper Functions (Internal to this translation unit) ---
    namespace { // Use an anonymous namespace for internal helpers

    void createDescriptorSetLayout() {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        //BIBI
        bindings.push_back({0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr});
        bindings.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        bindings.push_back({2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, nullptr});
        // Binding 3+: Vertex/Index/Material Storage Buffers (Add as needed)

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(g_Device, &layoutInfo, g_Allocator, &rtDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Ray Tracing descriptor set layout!");
        }
         std::cout << "Ray Tracing Descriptor Set Layout created." << std::endl;
    }

    void createPipelineLayout() {
        if (rtDescriptorSetLayout == VK_NULL_HANDLE) {
            throw std::runtime_error("RT Descriptor Set Layout must be created before pipeline layout!");
        }
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &rtDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, g_Allocator, &rtPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Ray Tracing pipeline layout!");
        }
        std::cout << "Ray Tracing Pipeline Layout created." << std::endl;
    }

    void allocateDescriptorSet() {
        if (rtDescriptorSetLayout == VK_NULL_HANDLE || g_DescriptorPool == VK_NULL_HANDLE) {
            throw std::runtime_error("Cannot allocate descriptor set without layout or pool!");
        }
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = g_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &rtDescriptorSetLayout;

        if (vkAllocateDescriptorSets(g_Device, &allocInfo, &rtDescriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Ray Tracing descriptor set!");
        }
         std::cout << "Ray Tracing Descriptor Set allocated." << std::endl;
    }

    } // end anonymous namespace

    // --- Public Namespace Functions ---

    void initialize() {
        // Only proceed if ray tracing is actually available
        if (!Kinesis::GUI::raytracing_available) {
            std::cout << "Ray Tracing not available on this hardware. Skipping RT Manager initialization." << std::endl;
            return;
        }
        if(g_Device == VK_NULL_HANDLE) {
             std::cerr << "Warning: Vulkan device not ready during RayTracingManager::initialize(). Skipping." << std::endl;
             return;
        }


        std::cout << "Initializing Ray Tracing Manager..." << std::endl;
        try {
            createDescriptorSetLayout();
            createPipelineLayout();
            allocateDescriptorSet();
            // Initialize other RT resources here (Pipeline, SBT, etc.)
            // e.g., createRayTracingPipeline();
            // e.g., createShaderBindingTable();
        } catch (const std::exception& e) {
            std::cerr << "Ray Tracing Manager Initialization failed: " << e.what() << std::endl;
            // Clean up partially created resources before re-throwing or exiting
            cleanup(); // Attempt cleanup
            throw;     // Re-throw
        }
         std::cout << "Ray Tracing Manager Initialized Successfully." << std::endl;
    }

    void cleanup() {
        // Check if resources were potentially initialized (RT available and device exists)
        // No need to check raytracing_available here, as handles will be VK_NULL_HANDLE if initialize skipped.
        if(g_Device == VK_NULL_HANDLE) {
            // Nothing to clean if device doesn't exist
            return;
        }

        std::cout << "Cleaning up Ray Tracing Manager..." << std::endl;

        // Destroy resources in reverse order of creation
        // vkDestroyPipeline(g_Device, rtPipeline, g_Allocator); // If pipeline was created
        // vkDestroyBuffer(g_Device, sbtBuffer, g_Allocator); // If SBT buffer was created
        // vkFreeMemory(g_Device, sbtBufferMemory, g_Allocator); // If SBT memory was allocated

        if (rtPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(g_Device, rtPipelineLayout, g_Allocator);
            rtPipelineLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Pipeline Layout destroyed." << std::endl;
        }
        if (rtDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout, g_Allocator);
            rtDescriptorSetLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Descriptor Set Layout destroyed." << std::endl;
        }
        // Descriptor sets are automatically freed when the pool is destroyed.
        // If you needed vkFreeDescriptorSets, call it here *before* destroying the layout/pool.
        rtDescriptorSet = VK_NULL_HANDLE; // Mark as cleaned

         std::cout << "Ray Tracing Manager Cleanup Finished." << std::endl;

    }

    // Call this AFTER tlas, outputImgView, camBuffer etc. are valid handles!
    void updateDescriptorSet(VkAccelerationStructureKHR tlas, VkImageView outputImgView, VkBuffer camBuffer, VkDeviceSize camBufSize /*, other resources...*/) {
        //BIBI
        // Only proceed if ray tracing was initialized
        if (rtDescriptorSet == VK_NULL_HANDLE) {
             if (Kinesis::GUI::raytracing_available) {
                 std::cerr << "Warning: Attempting to update RT descriptor set, but it was not allocated (check initialization)." << std::endl;
             }
            return; // Skip if not initialized (or RT not available)
        }

        // --- Check resource validity before updating ---
        if (tlas == VK_NULL_HANDLE || outputImgView == VK_NULL_HANDLE || camBuffer == VK_NULL_HANDLE) {
             std::cerr << "Warning: Skipping RT descriptor set update due to NULL resource handle." << std::endl;
            return;
        }

        //BIBI
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        VkWriteDescriptorSetAccelerationStructureKHR tlasWriteInfo{};
        VkDescriptorImageInfo imageInfo{};
        VkDescriptorBufferInfo cameraBufferInfo{};
        // Add other buffer/image info structs as needed

        // 1. Write for TLAS (Binding 0)
        //BIBI
        tlasWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        tlasWriteInfo.accelerationStructureCount = 1;
        tlasWriteInfo.pAccelerationStructures = &tlas;
        VkWriteDescriptorSet tlasWrite{};
        tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tlasWrite.dstSet = rtDescriptorSet;
        tlasWrite.dstBinding = 0;
        tlasWrite.dstArrayElement = 0;
        tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        tlasWrite.descriptorCount = 1;
        tlasWrite.pNext = &tlasWriteInfo;
        descriptorWrites.push_back(tlasWrite);

        // 2. Write for Output Image (Binding 1)
        imageInfo.imageView = outputImgView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = rtDescriptorSet;
        imageWrite.dstBinding = 1;
        imageWrite.dstArrayElement = 0;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;
        descriptorWrites.push_back(imageWrite);

        // 3. Write for Camera Buffer (Binding 2)
        cameraBufferInfo.buffer = camBuffer;
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = camBufSize;
        VkWriteDescriptorSet cameraWrite{};
        cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cameraWrite.dstSet = rtDescriptorSet;
        cameraWrite.dstBinding = 2;
        cameraWrite.dstArrayElement = 0;
        cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraWrite.descriptorCount = 1;
        cameraWrite.pBufferInfo = &cameraBufferInfo;
        descriptorWrites.push_back(cameraWrite);

        // 4. Add writes for other storage buffers...

        vkUpdateDescriptorSets(g_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

     void bind(VkCommandBuffer commandBuffer) {
        // Only bind if RT is available and resources are ready
        if (!Kinesis::GUI::raytracing_available || rtDescriptorSet == VK_NULL_HANDLE || rtPipelineLayout == VK_NULL_HANDLE /* || rtPipeline == VK_NULL_HANDLE */ ) {
            // Don't attempt to bind if RT isn't set up
            return;
        }

        // Bind the Ray Tracing Pipeline (assuming you have created rtPipeline)
        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

        // Bind the Ray Tracing Descriptor Set
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            rtPipelineLayout, // Use the RT pipeline layout
            0,                // First set index
            1,                // Number of sets to bind
            &rtDescriptorSet, // Pointer to the descriptor set handle
            0, nullptr);
    }


} 