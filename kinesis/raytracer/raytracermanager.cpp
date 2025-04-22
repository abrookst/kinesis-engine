// kinesis/raytracer/raytracermanager.cpp

#include <stdexcept>
#include <vector>
#include <array>
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

#include "kinesis.h" // Include kinesis.h for globals like g_Device, g_Allocator etc.
#include "RayTracerManager.h"
#include "GUI.h"
#include "mesh/mesh.h" // Needed for geometry info
#include "mesh/vertex.h" // Include Vertex definition
#include "gameobject.h" // Needed for gameObjects global
#include "window.h"     // For createBuffer, findMemoryType, etc.
#include "pipeline.h"   // For readFile
#include "buffer.h"
#include "gbuffer.h"    // For GBuffer data access in descriptor update

// --- Function Pointers Removed - using Volk loaded functions directly ---
// PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
// PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
// ... (rest of PFN definitions removed)


namespace Kinesis::RayTracerManager {

    // --- Define static/global variables declared extern in the header ---
    VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet rtDescriptorSet = VK_NULL_HANDLE; // Will be allocated per frame or updated
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{};
    std::vector<AccelerationStructure> blas;
    AccelerationStructure tlas{};
    VkBuffer instances_buffer = VK_NULL_HANDLE;
    VkDeviceMemory instances_buffer_memory = VK_NULL_HANDLE;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups{};
    VkPipeline rtPipeline = VK_NULL_HANDLE;
    ShaderBindingTableEntry rgenSBT{};
    ShaderBindingTableEntry missSBT{};
    ShaderBindingTableEntry chitSBT{};
    RTOutput rtOutput = {}; // Default initialize

    // Command pool for builds (can be specific to RTManager or shared)
    VkCommandPool buildCommandPool = VK_NULL_HANDLE; // Needs definition


    // --- Helper Functions ---
    uint64_t getBufferDeviceAddress(VkBuffer buffer) {
        // Check removed, assuming Volk loaded successfully
        // // if (!vkGetBufferDeviceAddressKHR) {
        // //      throw std::runtime_error("vkGetBufferDeviceAddressKHR function pointer not loaded!");
        // // }
        if (buffer == VK_NULL_HANDLE) {
             std::cerr << "Warning: Trying to get address of VK_NULL_HANDLE buffer." << std::endl;
             return 0; // Or handle as error
        }
        VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
        buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = buffer;
        // Directly call the function - Volk ensures it's available if loaded
        return vkGetBufferDeviceAddressKHR(g_Device, &buffer_device_address_info);
    }

    // Forward declarations for internal helpers if needed
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    ScratchBuffer create_scratch_buffer(VkDeviceSize size) {
        ScratchBuffer scratchBuffer;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        // Usage flags required for scratch buffer
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Use the global buffer creation helper
        Kinesis::Window::createBuffer(size, bufferInfo.usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     scratchBuffer.buffer, scratchBuffer.memory);
        scratchBuffer.address = getBufferDeviceAddress(scratchBuffer.buffer);
        return scratchBuffer;
    }

    void delete_scratch_buffer(ScratchBuffer &scratch_buffer) {
         if (g_Device == VK_NULL_HANDLE) return; // Avoid calls if device is null
        if (scratch_buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_Device, scratch_buffer.buffer, nullptr);
            scratch_buffer.buffer = VK_NULL_HANDLE;
        }
        if (scratch_buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(g_Device, scratch_buffer.memory, nullptr);
            scratch_buffer.memory = VK_NULL_HANDLE;
        }
        scratch_buffer.address = 0;
    }

    void delete_acceleration_structure(AccelerationStructure &acceleration_structure) {
         if (g_Device == VK_NULL_HANDLE) return; // Avoid calls if device is null

         // Call destroy function directly (Volk handles availability)
        if (acceleration_structure.structure != VK_NULL_HANDLE) { // Removed check for vkDestroyAccelerationStructureKHR assuming Volk loaded
            vkDestroyAccelerationStructureKHR(g_Device, acceleration_structure.structure, nullptr);
            acceleration_structure.structure = VK_NULL_HANDLE;
        }
        if (acceleration_structure.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_Device, acceleration_structure.buffer, nullptr);
            acceleration_structure.buffer = VK_NULL_HANDLE;
        }
        if (acceleration_structure.memory != VK_NULL_HANDLE) {
            vkFreeMemory(g_Device, acceleration_structure.memory, nullptr);
            acceleration_structure.memory = VK_NULL_HANDLE;
        }
        acceleration_structure.address = 0;
    }


    VkShaderModule createShaderModule(const std::string& filePath) {
        std::cout << "Loading shader: " << filePath << std::endl;
        if (!std::filesystem::exists(filePath)) {
             throw std::runtime_error("Shader file not found: " + filePath);
        }
        std::vector<char> code = Kinesis::Pipeline::readFile(filePath); // Use Pipeline's readFile
        if (code.empty()) {
             throw std::runtime_error("Shader file is empty: " + filePath);
        }
        std::cout << "  - Size: " << code.size() << " bytes" << std::endl;

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        if (code.size() % 4 != 0) {
             std::cerr << "Warning: Shader code size (" << code.size() << ") is not a multiple of 4 for " << filePath << ". This might cause issues." << std::endl;
        }
        // Ensure code size is correct even if warning is issued
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());


        VkShaderModule shaderModule;
        if (vkCreateShaderModule(g_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module for: " + filePath);
        }
        std::cout << "  - Module created successfully." << std::endl;
        return shaderModule;
    }

    void createRtOutputImage(VkExtent2D extent) {
        destroyRtOutputImage(); // Clean up existing if any

        rtOutput.format = VK_FORMAT_R16G16B16A16_SFLOAT; // Use HDR format

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = rtOutput.format;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // Needs to be Storage image for RGen shader and Sampled for composition pass
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // Add Transfer Src if needed for blitting/copying
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(g_Device, &imageInfo, nullptr, &rtOutput.image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RT output image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(g_Device, rtOutput.image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Kinesis::Window::findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(g_Device, &allocInfo, nullptr, &rtOutput.memory) != VK_SUCCESS) {
            vkDestroyImage(g_Device, rtOutput.image, nullptr); // Cleanup
            rtOutput.image = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate RT output image memory!");
        }
        vkBindImageMemory(g_Device, rtOutput.image, rtOutput.memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = rtOutput.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = rtOutput.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(g_Device, &viewInfo, nullptr, &rtOutput.view) != VK_SUCCESS) {
             vkDestroyImage(g_Device, rtOutput.image, nullptr); // Cleanup
             vkFreeMemory(g_Device, rtOutput.memory, nullptr); // Cleanup
             rtOutput.image = VK_NULL_HANDLE;
             rtOutput.memory = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create RT output image view!");
        }

        // Transition image layout to General for storage image usage
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL; // Layout for storage image access
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = rtOutput.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0; // No need to wait for previous writes
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Prepare for shader writes

        vkCmdPipelineBarrier(
            cmdBuf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Source stage
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, // Destination stage
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        endSingleTimeCommands(cmdBuf);

         std::cout << "RT Output Image created and transitioned to General layout." << std::endl;
    }

    void destroyRtOutputImage() {
         if (g_Device == VK_NULL_HANDLE) return; // Avoid calls if device is null
         if (rtOutput.view != VK_NULL_HANDLE) {
            vkDestroyImageView(g_Device, rtOutput.view, nullptr);
            rtOutput.view = VK_NULL_HANDLE;
        }
        if (rtOutput.image != VK_NULL_HANDLE) {
            vkDestroyImage(g_Device, rtOutput.image, nullptr);
            rtOutput.image = VK_NULL_HANDLE;
        }
        if (rtOutput.memory != VK_NULL_HANDLE) {
            vkFreeMemory(g_Device, rtOutput.memory, nullptr);
            rtOutput.memory = VK_NULL_HANDLE;
        }
    }

    void createRtDescriptorSetLayout() {
        if (g_Device == VK_NULL_HANDLE) return; // Avoid calls if device is null
        if (rtDescriptorSetLayout != VK_NULL_HANDLE) {
             vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout, nullptr);
             rtDescriptorSetLayout = VK_NULL_HANDLE;
        }

        std::vector<VkDescriptorSetLayoutBinding> bindings;

        // Binding 0: TLAS
        bindings.push_back({0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr});
        // Binding 1: Output Image (Storage)
        bindings.push_back({1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        // Binding 2: Camera UBO
        // Use the global descriptor set for camera now, so this binding might not be needed here.
        // Remove this binding if camera is in the global set (Set 0).
        // bindings.push_back({2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr});

        // Binding 3: G-Buffer Position (adjust binding index if camera removed)
        bindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        // Binding 4: G-Buffer Normal (adjust binding index if camera removed)
        bindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        // Binding 5: G-Buffer Albedo (adjust binding index if camera removed)
        bindings.push_back({5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        // Binding 6: G-Buffer Properties (adjust binding index if camera removed)
        bindings.push_back({6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
         // Add more bindings if needed (e.g., material buffers, scene info)

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        // Potentially add flags like VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT if needed

        if (vkCreateDescriptorSetLayout(g_Device, &layoutInfo, nullptr, &rtDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Ray Tracing descriptor set layout!");
        }
         std::cout << "RT Descriptor Set Layout created." << std::endl;
    }

    void createRayTracingPipeline() {
         assert(g_Device != VK_NULL_HANDLE && "Device must be valid");
         // Check removed, assuming Volk loaded successfully
         // // assert(vkCreateRayTracingPipelinesKHR != nullptr && "vkCreateRayTracingPipelinesKHR not loaded");

         // --- Create Pipeline Layout ---
         if (rtPipelineLayout != VK_NULL_HANDLE) {
             vkDestroyPipelineLayout(g_Device, rtPipelineLayout, nullptr);
             rtPipelineLayout = VK_NULL_HANDLE;
         }
         assert(rtDescriptorSetLayout != VK_NULL_HANDLE && "RT Descriptor Set Layout must be created first");
         // Use global set layout (Set 0) and RT set layout (Set 1)
         assert(Kinesis::globalSetLayout != VK_NULL_HANDLE && "Global set layout must exist");
         std::vector<VkDescriptorSetLayout> setLayouts = { Kinesis::globalSetLayout, rtDescriptorSetLayout };


         VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
         pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
         pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size()); // Now using two sets
         pipelineLayoutInfo.pSetLayouts = setLayouts.data();
         pipelineLayoutInfo.pushConstantRangeCount = 0; // No push constants for RT pipeline in this example
         pipelineLayoutInfo.pPushConstantRanges = nullptr;

         if (vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, nullptr, &rtPipelineLayout) != VK_SUCCESS) {
             throw std::runtime_error("Failed to create ray tracing pipeline layout!");
         }
         std::cout << "RT Pipeline Layout created." << std::endl;
         // --- Pipeline Layout Created ---


         std::cout << "Creating Ray Tracing Pipeline..." << std::endl;

         // Adjust shader paths based on execution directory if needed
         #if __APPLE__
             const std::string rgenShaderPath = "../../../../../../kinesis/assets/shaders/bin/raytrace.rgen.spv";
             const std::string missShaderPath = "../../../../../../kinesis/assets/shaders/bin/raytrace.rmiss.spv";
             const std::string chitShaderPath = "../../../../../../kinesis/assets/shaders/bin/raytrace.rchit.spv";
         #else
             const std::string rgenShaderPath = "../../../kinesis/assets/shaders/bin/raytrace.rgen.spv";
             const std::string missShaderPath = "../../../kinesis/assets/shaders/bin/raytrace.rmiss.spv";
             const std::string chitShaderPath = "../../../kinesis/assets/shaders/bin/raytrace.rchit.spv";
         #endif

         VkShaderModule rgenModule = VK_NULL_HANDLE;
         VkShaderModule missModule = VK_NULL_HANDLE;
         VkShaderModule chitModule = VK_NULL_HANDLE;

         try {
              rgenModule = createShaderModule(rgenShaderPath);
              missModule = createShaderModule(missShaderPath);
              chitModule = createShaderModule(chitShaderPath);
         } catch (const std::exception& e) {
              if (rgenModule) vkDestroyShaderModule(g_Device, rgenModule, nullptr);
              if (missModule) vkDestroyShaderModule(g_Device, missModule, nullptr);
              // Potential missing cleanup for chitModule if it failed after others succeeded
              if (chitModule) vkDestroyShaderModule(g_Device, chitModule, nullptr);
              std::cerr << "Failed to load ray tracing shaders: " << e.what() << std::endl;
              throw;
         }

         std::vector<VkPipelineShaderStageCreateInfo> stages;
         // RGen Stage
         VkPipelineShaderStageCreateInfo rgenStageInfo{};
         rgenStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
         rgenStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
         rgenStageInfo.module = rgenModule;
         rgenStageInfo.pName = "main";
         stages.push_back(rgenStageInfo);
         // Miss Stage
         VkPipelineShaderStageCreateInfo missStageInfo{};
         missStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
         missStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
         missStageInfo.module = missModule;
         missStageInfo.pName = "main";
         stages.push_back(missStageInfo);
         // CHit Stage
         VkPipelineShaderStageCreateInfo chitStageInfo{};
         chitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
         chitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
         chitStageInfo.module = chitModule;
         chitStageInfo.pName = "main";
         stages.push_back(chitStageInfo);

         // Shader Groups
         shader_groups.clear();
         // RGen Group (Index 0)
         VkRayTracingShaderGroupCreateInfoKHR rgenGroup{};
         rgenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
         rgenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
         rgenGroup.generalShader = 0; // Index of RGen stage in `stages`
         rgenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
         rgenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
         rgenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
         shader_groups.push_back(rgenGroup);
         // Miss Group (Index 1)
         VkRayTracingShaderGroupCreateInfoKHR missGroup{};
         missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
         missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
         missGroup.generalShader = 1; // Index of Miss stage in `stages`
         missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
         missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
         missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
         shader_groups.push_back(missGroup);
         // CHit Group (Index 2) - Triangle geometry uses this group
         VkRayTracingShaderGroupCreateInfoKHR chitGroup{};
         chitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
         chitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
         chitGroup.generalShader = VK_SHADER_UNUSED_KHR;
         chitGroup.closestHitShader = 2; // Index of CHit stage in `stages`
         chitGroup.anyHitShader = VK_SHADER_UNUSED_KHR; // Add AnyHit shader index if used
         chitGroup.intersectionShader = VK_SHADER_UNUSED_KHR; // Use for procedural geometry
         shader_groups.push_back(chitGroup);

         // Pipeline Create Info
         VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
         pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
         pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
         pipelineInfo.pStages = stages.data();
         pipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
         pipelineInfo.pGroups = shader_groups.data();
         pipelineInfo.maxPipelineRayRecursionDepth = 1; // Max recursion depth (e.g., 1 for primary rays only)
         pipelineInfo.layout = rtPipelineLayout; // Use the RT pipeline layout created above
         // pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // For pipeline derivatives
         // pipelineInfo.basePipelineIndex = -1;

         if (rtPipeline != VK_NULL_HANDLE) {
             vkDestroyPipeline(g_Device, rtPipeline, nullptr); // Destroy old pipeline if exists
             rtPipeline = VK_NULL_HANDLE;
         }

         // Call function directly (Volk ensures availability)
         if (vkCreateRayTracingPipelinesKHR(g_Device, VK_NULL_HANDLE, g_PipelineCache, 1, &pipelineInfo, nullptr, &rtPipeline) != VK_SUCCESS) {
             // Cleanup modules and layout on failure
             vkDestroyShaderModule(g_Device, rgenModule, nullptr);
             vkDestroyShaderModule(g_Device, missModule, nullptr);
             vkDestroyShaderModule(g_Device, chitModule, nullptr);
             vkDestroyPipelineLayout(g_Device, rtPipelineLayout, nullptr); // Clean up layout on failure
             rtPipelineLayout = VK_NULL_HANDLE;
             throw std::runtime_error("Failed to create ray tracing pipeline!");
         }

         // Cleanup shader modules - they are no longer needed after pipeline creation
         vkDestroyShaderModule(g_Device, rgenModule, nullptr);
         vkDestroyShaderModule(g_Device, missModule, nullptr);
         vkDestroyShaderModule(g_Device, chitModule, nullptr);

         std::cout << "Ray Tracing Pipeline created successfully." << std::endl;
    }

    // Helper to Create and Upload SBT Entry
    void createSBTEntry(ShaderBindingTableEntry& sbtEntry, uint32_t groupIndex, uint32_t handleSize, uint32_t groupHandleAlignment, const uint8_t* shaderHandleStorage) {
        // SBT entries need specific usage flags and alignment
        const VkBufferUsageFlags sbtBufferUsageFlags =
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Need transfer destination

        // The size of one entry in the SBT must be aligned to shaderGroupHandleAlignment
        const VkDeviceSize sbtEntrySizeAligned = Kinesis::Buffer::getAlignment(handleSize, groupHandleAlignment);

        // Destroy old buffer/memory if it exists
        if (sbtEntry.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_Device, sbtEntry.buffer, nullptr);
            sbtEntry.buffer = VK_NULL_HANDLE;
        }
        if (sbtEntry.memory != VK_NULL_HANDLE) {
            vkFreeMemory(g_Device, sbtEntry.memory, nullptr);
            sbtEntry.memory = VK_NULL_HANDLE;
        }


        // Create Buffer using the helper function
        Kinesis::Window::createBuffer(
            sbtEntrySizeAligned, // Use aligned size
            sbtBufferUsageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // Use device local memory for performance
            sbtEntry.buffer,
            sbtEntry.memory
        );

        // Create staging buffer for upload
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        Kinesis::Window::createBuffer(
            handleSize, // Size of one raw handle
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory
        );

        // Map staging buffer and copy the specific handle data
        void* mappedData;
        vkMapMemory(g_Device, stagingMemory, 0, handleSize, 0, &mappedData);
        // Copy the handle for the specified groupIndex from the retrieved storage
        memcpy(mappedData, shaderHandleStorage + groupIndex * handleSize, handleSize);
        vkUnmapMemory(g_Device, stagingMemory);

        // Copy from staging buffer to the start of the SBT buffer
        VkCommandBuffer cmdBuf = beginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0; // Copy to the beginning of the SBT entry buffer
        copyRegion.size = handleSize; // Copy only the raw handle size
        vkCmdCopyBuffer(cmdBuf, stagingBuffer, sbtEntry.buffer, 1, &copyRegion);
        endSingleTimeCommands(cmdBuf);

        // Cleanup staging buffer
        vkDestroyBuffer(g_Device, stagingBuffer, nullptr);
        vkFreeMemory(g_Device, stagingMemory, nullptr);


        // Set up address region for vkCmdTraceRaysKHR
        sbtEntry.addressRegion.deviceAddress = getBufferDeviceAddress(sbtEntry.buffer);
        sbtEntry.addressRegion.stride = sbtEntrySizeAligned; // Stride must be the aligned size
        sbtEntry.addressRegion.size = sbtEntrySizeAligned;   // Size must be the aligned size
    }


    void createShaderBindingTable() {
         assert(rtPipeline != VK_NULL_HANDLE && "Ray tracing pipeline must be created before SBT");
         // Check removed, assuming Volk loaded successfully
         // // assert(vkGetRayTracingShaderGroupHandlesKHR != nullptr && "vkGetRayTracingShaderGroupHandlesKHR not loaded");
         assert(!shader_groups.empty() && "Shader groups must be created before SBT");


         std::cout << "Creating Shader Binding Table..." << std::endl;

         const uint32_t handleSize = rt_pipeline_properties.shaderGroupHandleSize;
         const uint32_t handleAlignment = rt_pipeline_properties.shaderGroupHandleAlignment; // Renamed for clarity
         const uint32_t groupCount = static_cast<uint32_t>(shader_groups.size());

         // Get all shader group handles from the pipeline
         const uint32_t dataSize = groupCount * handleSize;
         std::vector<uint8_t> shaderHandleStorage(dataSize);

         // Call function directly (Volk ensures availability)
         if (vkGetRayTracingShaderGroupHandlesKHR(g_Device, rtPipeline, 0, groupCount, dataSize, shaderHandleStorage.data()) != VK_SUCCESS) {
             throw std::runtime_error("Failed to get ray tracing shader group handles!");
         }

         // --- Create SBT entries ---
         // Assuming Group Indices: 0=RGen, 1=Miss, 2=CHit
         // Use handleAlignment for alignment parameter
         createSBTEntry(rgenSBT, 0, handleSize, handleAlignment, shaderHandleStorage.data());
         createSBTEntry(missSBT, 1, handleSize, handleAlignment, shaderHandleStorage.data());
         createSBTEntry(chitSBT, 2, handleSize, handleAlignment, shaderHandleStorage.data());
         // Create other entries (ahitSBT, callableSBT) if needed, adjusting indices

         std::cout << "Shader Binding Table created successfully." << std::endl;
         std::cout << "  - RGen Address: " << rgenSBT.addressRegion.deviceAddress << ", Stride: " << rgenSBT.addressRegion.stride << ", Size: " << rgenSBT.addressRegion.size << std::endl;
         std::cout << "  - Miss Address: " << missSBT.addressRegion.deviceAddress << ", Stride: " << missSBT.addressRegion.stride << ", Size: " << missSBT.addressRegion.size << std::endl;
         std::cout << "  - CHit Address: " << chitSBT.addressRegion.deviceAddress << ", Stride: " << chitSBT.addressRegion.stride << ", Size: " << chitSBT.addressRegion.size << std::endl;
     }


    // --- initialize ---
    void initialize(VkExtent2D extent) {
        if (!Kinesis::GUI::raytracing_available || g_Device == VK_NULL_HANDLE) {
            std::cout << "Ray Tracing not available or device not ready. Skipping RT Manager initialization." << std::endl;
            return;
        }

        // --- Load KHR function pointers (REMOVED - using Volk now) ---
        // Function pointers loaded via volkLoadDevice in window.cpp

        // Create command pool for builds (if not already created elsewhere)
        if (buildCommandPool == VK_NULL_HANDLE) {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = g_QueueFamily; // Use global queue family
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow resetting individual buffers
            if (vkCreateCommandPool(g_Device, &poolInfo, nullptr, &buildCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool for AS builds!");
            }
        }

        // Get RT Properties & Features
        // Ensure these are queried using VkPhysicalDeviceProperties2 after selecting physical device
        rt_pipeline_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        as_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        VkPhysicalDeviceProperties2 deviceProps2{};
        deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProps2.pNext = &rt_pipeline_properties;
        // Chain the features structure to the properties structure if needed elsewhere,
        // or query separately using VkPhysicalDeviceFeatures2 if only features are needed here.
        // For now, assuming properties are needed (e.g., for SBT handle sizes).
        // If features were chained during device creation, they might already be available.
        // Re-querying properties here is safe.
        vkGetPhysicalDeviceProperties2(g_PhysicalDevice, &deviceProps2);
        // Query features separately if needed and not already stored/chained elsewhere
        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &as_features; // Link features struct
        vkGetPhysicalDeviceFeatures2(g_PhysicalDevice, &deviceFeatures2);


        std::cout << "Initializing Ray Tracing Manager..." << std::endl;
        try {
            // 1. Create Layouts (Global layout created in kinesis.cpp)
            createRtDescriptorSetLayout(); // Creates rtDescriptorSetLayout (Set 1)

            // 2. Create Output Image
            createRtOutputImage(extent); // Creates rtOutput

            // 3. Build Acceleration Structures
            create_blas(); // Creates blas vector
            create_tlas(true); // Creates tlas, allows updates

            // 4. Create RT Pipeline (Requires layouts)
            createRayTracingPipeline(); // Creates rtPipeline and rtPipelineLayout

            // 5. Create Shader Binding Table (Requires pipeline)
            createShaderBindingTable(); // Creates SBT entries (rgenSBT, missSBT, chitSBT)

            // 6. Initial Descriptor Set Allocation/Update (will be done per-frame in run loop)
            // allocateAndUpdateRtDescriptorSet is called later with frame-specific data

        } catch (const std::exception& e) {
             std::cerr << "Ray Tracing Manager Initialization failed: " << e.what() << std::endl;
             cleanup(); // Attempt cleanup on failure
             throw;
        }
        std::cout << "Ray Tracing Manager Initialized Successfully." << std::endl;
    }

    // --- cleanup ---
    void cleanup() {
         // Check device validity before cleanup
        if (g_Device == VK_NULL_HANDLE /* Removed check for !Kinesis::GUI::raytracing_available as cleanup might be needed even if RT init failed */) return;


        std::cout << "Cleaning up Ray Tracing Manager..." << std::endl;
        // Ensure all GPU operations are finished before destroying resources
        vkDeviceWaitIdle(g_Device);

        // Destroy SBT Buffers
        auto destroySBTEntry = [&](ShaderBindingTableEntry& entry) {
             if (g_Device == VK_NULL_HANDLE) return; // Check again inside lambda
            if (entry.buffer != VK_NULL_HANDLE) vkDestroyBuffer(g_Device, entry.buffer, nullptr);
            if (entry.memory != VK_NULL_HANDLE) vkFreeMemory(g_Device, entry.memory, nullptr);
            entry = {}; // Reset struct
        };
        destroySBTEntry(rgenSBT);
        destroySBTEntry(missSBT);
        destroySBTEntry(chitSBT);
         std::cout << "  - SBTs destroyed." << std::endl;


        // Destroy RT Pipeline
        if (rtPipeline != VK_NULL_HANDLE) {
             vkDestroyPipeline(g_Device, rtPipeline, nullptr);
             rtPipeline = VK_NULL_HANDLE;
             std::cout << "  - RT Pipeline destroyed." << std::endl;
        }

         // Destroy Layouts
         // Global layout (Set 0) is cleaned up in kinesis.cpp
        if (rtPipelineLayout != VK_NULL_HANDLE) {
             vkDestroyPipelineLayout(g_Device, rtPipelineLayout, g_Allocator); // Use allocator if specified at creation
             rtPipelineLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Pipeline Layout destroyed." << std::endl;
        }
        if (rtDescriptorSetLayout != VK_NULL_HANDLE) {
             vkDestroyDescriptorSetLayout(g_Device, rtDescriptorSetLayout, g_Allocator); // Use allocator
             rtDescriptorSetLayout = VK_NULL_HANDLE;
             std::cout << "  - RT Descriptor Set Layout destroyed." << std::endl;
        }
        // Descriptor set (rtDescriptorSet) is freed when the pool (g_DescriptorPool) is destroyed


        // Destroy Acceleration Structures
        if (tlas.structure) {
            delete_acceleration_structure(tlas); // Handles buffer/memory too
             std::cout << "  - TLAS destroyed." << std::endl;
        } else {
             // Ensure buffer/memory are cleaned up even if structure creation failed
             if (tlas.buffer != VK_NULL_HANDLE) vkDestroyBuffer(g_Device, tlas.buffer, nullptr);
             if (tlas.memory != VK_NULL_HANDLE) vkFreeMemory(g_Device, tlas.memory, nullptr);
             tlas = {}; // Reset struct
        }

        if (instances_buffer != VK_NULL_HANDLE) {
             vkDestroyBuffer(g_Device, instances_buffer, nullptr);
             instances_buffer = VK_NULL_HANDLE;
        }
         if (instances_buffer_memory != VK_NULL_HANDLE) {
              vkFreeMemory(g_Device, instances_buffer_memory, nullptr);
              instances_buffer_memory = VK_NULL_HANDLE;
              std::cout << "  - Instance Buffer destroyed." << std::endl;
         }

        for (auto& b : blas) {
             delete_acceleration_structure(b); // Handles buffer/memory too
        }
        blas.clear();
         std::cout << "  - BLASes destroyed." << std::endl;

         // Destroy RT Output Image
         destroyRtOutputImage();
         std::cout << "  - RT Output Image destroyed." << std::endl;


        // Destroy Build Command Pool (if created by this manager)
        if (buildCommandPool != VK_NULL_HANDLE) {
             vkDestroyCommandPool(g_Device, buildCommandPool, nullptr);
             buildCommandPool = VK_NULL_HANDLE;
             std::cout << "  - Build Command Pool destroyed." << std::endl;
        }

        rtDescriptorSet = VK_NULL_HANDLE; // Reset the handle (memory freed with pool)
        std::cout << "Ray Tracing Manager Cleanup Finished." << std::endl;
    }

    // --- allocateAndUpdateRtDescriptorSet ---
    // Updates the RT-specific descriptor set (Set 1)
     void allocateAndUpdateRtDescriptorSet(VkAccelerationStructureKHR tlasHandle, VkBuffer /*camBuffer*/, VkDeviceSize /*camBufSize*/) {
         assert(rtDescriptorSetLayout != VK_NULL_HANDLE && "RT Descriptor Set Layout must be created first");
         assert(g_DescriptorPool != VK_NULL_HANDLE && "Global Descriptor Pool is null");
         assert(rtOutput.view != VK_NULL_HANDLE && "RT Output Image View must be created first");
         // Assert for GBuffer resources needed
         assert(Kinesis::GBuffer::sampler != VK_NULL_HANDLE && "G-Buffer Sampler missing");
         assert(Kinesis::GBuffer::positionAttachment.view != VK_NULL_HANDLE && "G-Buffer Position View missing");
         assert(Kinesis::GBuffer::normalAttachment.view != VK_NULL_HANDLE && "G-Buffer Normal View missing");
         assert(Kinesis::GBuffer::albedoAttachment.view != VK_NULL_HANDLE && "G-Buffer Albedo View missing");
         assert(Kinesis::GBuffer::propertiesAttachment.view != VK_NULL_HANDLE && "G-Buffer Properties View missing");
         assert(tlasHandle != VK_NULL_HANDLE && "TLAS Handle missing");
         // Removed assertions for camBuffer/camBufSize as camera is now in global set

         // Allocate the descriptor set if it hasn't been allocated yet
         // We typically reuse the same set handle and just update its contents each frame.
         if (rtDescriptorSet == VK_NULL_HANDLE) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = g_DescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &rtDescriptorSetLayout; // Use the RT layout
            if (vkAllocateDescriptorSets(g_Device, &allocInfo, &rtDescriptorSet) != VK_SUCCESS) {
                 throw std::runtime_error("Failed to allocate Ray Tracing descriptor set!");
            }
            std::cout << "RT Descriptor Set Allocated." << std::endl;
         }

         // --- Prepare Descriptor Writes for the RT set (Set 1) ---
         std::vector<VkWriteDescriptorSet> descriptorWrites;
         std::vector<VkDescriptorImageInfo> gbufferInfos(4); // Need to keep image infos alive

         // 0: TLAS
         VkWriteDescriptorSetAccelerationStructureKHR tlasWriteInfo{}; // Keep this alive
         tlasWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
         tlasWriteInfo.accelerationStructureCount = 1;
         tlasWriteInfo.pAccelerationStructures = &tlasHandle; // Use the passed handle
         VkWriteDescriptorSet tlasWrite{};
         tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         tlasWrite.dstSet = rtDescriptorSet; // Update the existing RT set
         tlasWrite.dstBinding = 0; // Binding 0 in the RT layout
         tlasWrite.dstArrayElement = 0;
         tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
         tlasWrite.descriptorCount = 1;
         tlasWrite.pNext = &tlasWriteInfo; // Link the AS info
         descriptorWrites.push_back(tlasWrite);

         // 1: Output Image (Storage)
         VkDescriptorImageInfo outputImageInfo{}; // Keep this alive
         outputImageInfo.imageView = rtOutput.view;
         outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // Must be GENERAL for storage write
         VkWriteDescriptorSet outputImageWrite{};
         outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         outputImageWrite.dstSet = rtDescriptorSet;
         outputImageWrite.dstBinding = 1; // Binding 1 in the RT layout
         outputImageWrite.dstArrayElement = 0;
         outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
         outputImageWrite.descriptorCount = 1;
         outputImageWrite.pImageInfo = &outputImageInfo;
         descriptorWrites.push_back(outputImageWrite);

         // 2: Camera UBO (REMOVED - Now in global set 0)

         // --- Bindings adjusted for removed camera ---

         // 3: G-Buffer Position (now binding 3, check layout definition)
         gbufferInfos[0].sampler = Kinesis::GBuffer::sampler;
         gbufferInfos[0].imageView = Kinesis::GBuffer::positionAttachment.view;
         gbufferInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Layout must match usage
         // 4: G-Buffer Normal (now binding 4)
         gbufferInfos[1].sampler = Kinesis::GBuffer::sampler;
         gbufferInfos[1].imageView = Kinesis::GBuffer::normalAttachment.view;
         gbufferInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         // 5: G-Buffer Albedo (now binding 5)
         gbufferInfos[2].sampler = Kinesis::GBuffer::sampler;
         gbufferInfos[2].imageView = Kinesis::GBuffer::albedoAttachment.view;
         gbufferInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         // 6: G-Buffer Properties (now binding 6)
         gbufferInfos[3].sampler = Kinesis::GBuffer::sampler;
         gbufferInfos[3].imageView = Kinesis::GBuffer::propertiesAttachment.view;
         gbufferInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

         VkWriteDescriptorSet gbufferWriteBase{}; // Base struct for common fields
         gbufferWriteBase.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         gbufferWriteBase.dstSet = rtDescriptorSet; // Target the RT set
         gbufferWriteBase.dstArrayElement = 0;
         gbufferWriteBase.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         gbufferWriteBase.descriptorCount = 1;

         // Position Write (Binding 3)
         VkWriteDescriptorSet gbufferPosWrite = gbufferWriteBase;
         gbufferPosWrite.dstBinding = 3; // Use correct binding index
         gbufferPosWrite.pImageInfo = &gbufferInfos[0];
         descriptorWrites.push_back(gbufferPosWrite);
         // Normal Write (Binding 4)
         VkWriteDescriptorSet gbufferNormWrite = gbufferWriteBase;
         gbufferNormWrite.dstBinding = 4;
         gbufferNormWrite.pImageInfo = &gbufferInfos[1];
         descriptorWrites.push_back(gbufferNormWrite);
         // Albedo Write (Binding 5)
         VkWriteDescriptorSet gbufferAlbWrite = gbufferWriteBase;
         gbufferAlbWrite.dstBinding = 5;
         gbufferAlbWrite.pImageInfo = &gbufferInfos[2];
         descriptorWrites.push_back(gbufferAlbWrite);
         // Properties Write (Binding 6)
         VkWriteDescriptorSet gbufferPropWrite = gbufferWriteBase;
         gbufferPropWrite.dstBinding = 6;
         gbufferPropWrite.pImageInfo = &gbufferInfos[3];
         descriptorWrites.push_back(gbufferPropWrite);


         // --- Update the Descriptor Set ---
         vkUpdateDescriptorSets(g_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
         // std::cout << "RT Descriptor Set updated." << std::endl; // Can be noisy per frame
    }

    // --- bind ---
    // Binds the RT pipeline and its descriptor sets
    void bind(VkCommandBuffer commandBuffer, VkDescriptorSet globalSet) { // Pass global set
        if (!Kinesis::GUI::raytracing_available || rtPipeline == VK_NULL_HANDLE || rtPipelineLayout == VK_NULL_HANDLE || rtDescriptorSet == VK_NULL_HANDLE || globalSet == VK_NULL_HANDLE) {
             std::cerr << "Warning: Attempting to bind uninitialized ray tracing resources or missing global set!" << std::endl;
            return;
        }
        // Bind the RT pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

        // Bind descriptor sets: Set 0 = global, Set 1 = RT specific
        std::array<VkDescriptorSet, 2> descriptorSetsToBind = { globalSet, rtDescriptorSet };
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            rtPipelineLayout, // The layout compatible with both sets
            0, // First set index to bind
            static_cast<uint32_t>(descriptorSetsToBind.size()), // Number of sets to bind
            descriptorSetsToBind.data(), // Pointer to array of sets
            0, nullptr); // No dynamic offsets
    }

    // --- traceRays ---
    void traceRays(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height) {
        // Add assertions for function pointer and SBT validity if desired
        // // assert(vkCmdTraceRaysKHR != nullptr); // Not needed with Volk
        assert(rgenSBT.buffer != VK_NULL_HANDLE);
        assert(missSBT.buffer != VK_NULL_HANDLE);
        assert(chitSBT.buffer != VK_NULL_HANDLE);

        // Call function directly (Volk ensures availability)
        vkCmdTraceRaysKHR(
            commandBuffer,
            &rgenSBT.addressRegion, // RayGen SBT entry info
            &missSBT.addressRegion, // Miss SBT entry info
            &chitSBT.addressRegion, // Hit Group SBT entry info
            nullptr, // Callable SBT entry info (if used)
            width, height, 1); // Dimensions of the ray dispatch
    }

     // --- Single Time Command Helpers ---
    VkCommandBuffer beginSingleTimeCommands() {
        assert(buildCommandPool != VK_NULL_HANDLE && "Build command pool not initialized");
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = buildCommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(g_Device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
             throw std::runtime_error("Failed to allocate single-time command buffer!");
        }


        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Execute once then reset/free

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
             vkFreeCommandBuffers(g_Device, buildCommandPool, 1, &commandBuffer); // Cleanup allocated buffer
            throw std::runtime_error("Failed to begin single-time command buffer!");
        }


        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
         assert(g_Queue != VK_NULL_HANDLE && "Graphics queue not initialized");
         assert(buildCommandPool != VK_NULL_HANDLE && "Build command pool not initialized");

         if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
              // Don't free buffer here if caller might retry, but log error
              // vkFreeCommandBuffers(g_Device, buildCommandPool, 1, &commandBuffer);
             throw std::runtime_error("Failed to end single-time command buffer!");
         }


        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // Use a fence to wait for completion
        VkFence fence;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(g_Device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
             // vkFreeCommandBuffers(g_Device, buildCommandPool, 1, &commandBuffer); // Cleanup command buffer if fence fails
             throw std::runtime_error("Failed to create fence for single-time command buffer!");
        }

        // Submit to the queue
        VkResult queueResult = vkQueueSubmit(g_Queue, 1, &submitInfo, fence);
        if (queueResult != VK_SUCCESS) {
             vkDestroyFence(g_Device, fence, nullptr); // Cleanup fence
             // vkFreeCommandBuffers(g_Device, buildCommandPool, 1, &commandBuffer); // Cleanup command buffer
            throw std::runtime_error("Failed to submit single-time command buffer! Error code: " + std::to_string(queueResult));
        }

        // Wait for the fence to signal completion
        vkWaitForFences(g_Device, 1, &fence, VK_TRUE, UINT64_MAX); // Wait indefinitely

        // Cleanup fence and command buffer
        vkDestroyFence(g_Device, fence, nullptr);
        vkFreeCommandBuffers(g_Device, buildCommandPool, 1, &commandBuffer);
    }

    // --- create_blas ---
    void create_blas() {
        // Clean up existing BLAS first
         for (auto& b : blas) delete_acceleration_structure(b);
         blas.clear();

        uint32_t blasIndex = 0; // Keep track of which blas corresponds to which game object
        for (const auto& gameObject : Kinesis::gameObjects) {
            if (!gameObject.model || !gameObject.model->getMesh() || gameObject.model->getMesh()->numVertices() == 0) continue;

            // 1. Get Geometry Data Pointers/Addresses
            VkBuffer vertexBuffer = gameObject.model->getVertexBuffer();
            VkBuffer indexBuffer = gameObject.model->getIndexBuffer(); // Get index buffer
            bool hasIndices = gameObject.model->getMesh()->hasIndices();

            if (vertexBuffer == VK_NULL_HANDLE || (hasIndices && indexBuffer == VK_NULL_HANDLE)) {
                 std::cerr << "Warning: Skipping BLAS creation for GameObject '" << gameObject.name << "' due to missing buffers." << std::endl;
                 continue;
            }

            uint64_t vertexBufferAddress = getBufferDeviceAddress(vertexBuffer);
            uint64_t indexBufferAddress = hasIndices ? getBufferDeviceAddress(indexBuffer) : 0;
            uint32_t vertexCount = gameObject.model->getMesh()->numVertices();
            uint32_t indexCount = gameObject.model->getMesh()->numIndices();
            // Calculate primitive count based on indices or vertices
            uint32_t primitiveCount = hasIndices ? (indexCount / 3) : (vertexCount / 3);

            if (primitiveCount == 0) {
                 std::cerr << "Warning: Skipping BLAS creation for GameObject '" << gameObject.name << "' due to zero primitives." << std::endl;
                 continue;
            }


            // 2. Define Acceleration Structure Geometry (Triangles)
            VkAccelerationStructureGeometryKHR accelGeom{};
            accelGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            accelGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            accelGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR; // Assume opaque for now, can be based on material later
            accelGeom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            accelGeom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // Match Kinesis::Mesh::Vertex position
            accelGeom.geometry.triangles.vertexData.deviceAddress = vertexBufferAddress;
            accelGeom.geometry.triangles.vertexStride = sizeof(Kinesis::Mesh::Vertex); // Stride is the size of the vertex struct
            accelGeom.geometry.triangles.maxVertex = vertexCount -1; // Highest vertex index used

            if (hasIndices) {
                accelGeom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32; // Assuming 32-bit indices
                accelGeom.geometry.triangles.indexData.deviceAddress = indexBufferAddress;
            } else {
                 accelGeom.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR; // Not using indices
                 accelGeom.geometry.triangles.indexData.deviceAddress = 0;
            }
            accelGeom.geometry.triangles.transformData = {}; // No transform for BLAS geometry itself

            // 3. Get Build Sizes
            VkAccelerationStructureBuildGeometryInfoKHR buildGeomInfo{};
            buildGeomInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildGeomInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            // Prefer fast trace, allow updates if needed later (though BLAS updates are less common)
            buildGeomInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; // Build mode
            buildGeomInfo.geometryCount = 1; // One geometry description per BLAS for simplicity
            buildGeomInfo.pGeometries = &accelGeom;

            VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
            buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            // Call function directly (Volk ensures availability)
            vkGetAccelerationStructureBuildSizesKHR(
                g_Device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, // Build on device
                &buildGeomInfo,
                &primitiveCount, // Number of triangles/primitives
                &buildSizesInfo
            );

            // 4. Create BLAS Buffer and AS Object
            AccelerationStructure blasEntry; // Create a new entry for this object
            Kinesis::Window::createBuffer(buildSizesInfo.accelerationStructureSize,
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        blasEntry.buffer, blasEntry.memory);

            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.buffer = blasEntry.buffer;
            createInfo.size = buildSizesInfo.accelerationStructureSize;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
             // Call function directly (Volk ensures availability)
            if(vkCreateAccelerationStructureKHR(g_Device, &createInfo, nullptr, &blasEntry.structure) != VK_SUCCESS){
                  // Cleanup buffer/memory if AS creation fails
                  delete_acceleration_structure(blasEntry); // Use helper to clean up
                 throw std::runtime_error("Failed to create BLAS for GameObject '" + gameObject.name + "'!");
            }
            // Get the device address *after* the AS is created and bound to the buffer implicitly
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = blasEntry.structure;
            blasEntry.address = vkGetAccelerationStructureDeviceAddressKHR(g_Device, &addressInfo); // Get address of the AS object


            // 5. Create Scratch Buffer
            ScratchBuffer scratch = create_scratch_buffer(buildSizesInfo.buildScratchSize);

            // 6. Build BLAS on GPU using a command buffer
            VkCommandBuffer cmdBuf = beginSingleTimeCommands();

             // Update buildGeomInfo for the build command
             buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
             buildGeomInfo.dstAccelerationStructure = blasEntry.structure; // Target AS object
             buildGeomInfo.scratchData.deviceAddress = scratch.address;   // Scratch buffer address

             // Define build range info (describes the primitives to build)
             VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
             buildRangeInfo.primitiveCount = primitiveCount;
             buildRangeInfo.primitiveOffset = 0; // Offset into index/vertex buffer
             buildRangeInfo.firstVertex = 0;     // Offset for non-indexed geometry
             buildRangeInfo.transformOffset = 0; // Offset for transform data (usually 0 for BLAS)
             const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

             // Call function directly (Volk ensures availability)
            vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildGeomInfo, &pBuildRangeInfo);

            // Barrier: Ensure BLAS build completes before scratch buffer is destroyed/reused
            // and before the BLAS is used in a TLAS build.
            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR; // Write finished
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;  // Ready for read (TLAS build, shader access)
            vkCmdPipelineBarrier(cmdBuf,
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, // Source stage
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, // Dest stages
                                 0, // No dependency flags needed usually
                                 1, &barrier, // Memory barrier
                                 0, nullptr, // Buffer barriers
                                 0, nullptr); // Image barriers


            endSingleTimeCommands(cmdBuf); // Submit and wait for completion

            // 7. Cleanup Scratch Buffer
            delete_scratch_buffer(scratch);

            // 8. Store BLAS
            blas.push_back(blasEntry); // Add the newly created BLAS to the vector
            blasIndex++; // Increment for the next object
        }
        std::cout << "Created " << blas.size() << " BLAS objects." << std::endl;
    }

    // --- create_tlas ---
    void create_tlas(bool allow_update) {
         // Delete previous TLAS and instance buffer if they exist
         if (tlas.structure != VK_NULL_HANDLE) {
             delete_acceleration_structure(tlas); // Cleans up buffer/memory too
             tlas = {}; // Reset struct
         }
         if (instances_buffer != VK_NULL_HANDLE) {
              vkDestroyBuffer(g_Device, instances_buffer, nullptr);
              instances_buffer = VK_NULL_HANDLE;
         }
          if (instances_buffer_memory != VK_NULL_HANDLE) {
               vkFreeMemory(g_Device, instances_buffer_memory, nullptr);
               instances_buffer_memory = VK_NULL_HANDLE;
          }


        // Create instance descriptions for each object that has a corresponding BLAS
        std::vector<VkAccelerationStructureInstanceKHR> instances;
        uint32_t customInstanceIndex = 0; // Track index for shaders (e.g., material lookup)
        // Ensure blas vector size matches gameObjects or use a map
        if (blas.size() != Kinesis::gameObjects.size()) {
             std::cerr << "Warning: Mismatch between number of BLAS (" << blas.size()
                       << ") and game objects (" << Kinesis::gameObjects.size()
                       << "). TLAS might be incomplete or incorrect." << std::endl;
             // Adjust logic here, maybe map BLAS to objects differently
        }

        for (size_t i = 0; i < Kinesis::gameObjects.size(); ++i) {
             // Ensure object has model and a corresponding BLAS was created
             if (!gameObjects[i].model || i >= blas.size() || blas[i].address == 0) continue;

             VkAccelerationStructureInstanceKHR instance{};
             // Convert glm::mat4 to VkTransformMatrixKHR (row-major)
             glm::mat4 modelMatrix = gameObjects[i].transform.mat4();
             // Vulkan expects row-major, glm is column-major by default, so transpose
             glm::mat4 transposed = glm::transpose(modelMatrix);
             memcpy(&instance.transform, &transposed, sizeof(VkTransformMatrixKHR));

             instance.instanceCustomIndex = customInstanceIndex++; // Unique ID accessible in shaders
             instance.mask = 0xFF; // Visibility mask (default: visible to all rays)
             // Offset into the SBT hit group records.
             // Simple case: All instances use the same hit group (index 2 from group creation), offset 0.
             // Complex case: Different materials -> different hit groups -> different offsets.
             instance.instanceShaderBindingTableRecordOffset = 0;
             instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // Example: Disable backface culling for this instance
             instance.accelerationStructureReference = blas[i].address; // Address of the BLAS for this instance
             instances.push_back(instance);
        }

        if (instances.empty()) {
             std::cerr << "Warning: No valid instances with corresponding BLAS found. Skipping TLAS build." << std::endl;
             return; // Nothing to build
        }

        // Create and upload instances buffer
        VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();
        // Usage flags for instance buffer
        const VkBufferUsageFlags instanceBufferUsage =
             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
             VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Need transfer destination

        // Use helper to create device-local buffer
         Kinesis::Window::createBuffer(instanceBufferSize, instanceBufferUsage,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    instances_buffer, instances_buffer_memory);

         // Upload instance data (using staging buffer for device-local memory)
         VkBuffer stagingBuffer;
         VkDeviceMemory stagingMemory;
         Kinesis::Window::createBuffer(instanceBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);
         void* data;
         vkMapMemory(g_Device, stagingMemory, 0, instanceBufferSize, 0, &data);
         memcpy(data, instances.data(), instanceBufferSize);
         vkUnmapMemory(g_Device, stagingMemory);

         VkCommandBuffer cmdBufCopy = beginSingleTimeCommands();
         VkBufferCopy copyRegion{};
         copyRegion.size = instanceBufferSize;
         vkCmdCopyBuffer(cmdBufCopy, stagingBuffer, instances_buffer, 1, &copyRegion);
         // Barrier: Ensure copy finishes before TLAS build reads the instance buffer
          VkBufferMemoryBarrier bufferBarrier = {};
          bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
          bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          bufferBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR; // Read by AS build
          bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          bufferBarrier.buffer = instances_buffer;
          bufferBarrier.offset = 0;
          bufferBarrier.size = instanceBufferSize;
          vkCmdPipelineBarrier(cmdBufCopy,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, // Source stage
                               VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, // Destination stage
                               0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

         endSingleTimeCommands(cmdBufCopy); // Submits and waits

         // Clean up staging buffer
         vkDestroyBuffer(g_Device, stagingBuffer, nullptr);
         vkFreeMemory(g_Device, stagingMemory, nullptr);

        uint64_t instanceBufferAddr = getBufferDeviceAddress(instances_buffer);

        // Define TLAS geometry (references the instances buffer)
        VkAccelerationStructureGeometryKHR tlasGeometry{};
        tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR; // Usually opaque for TLAS
        tlasGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE; // Data is a packed array
        tlasGeometry.geometry.instances.data.deviceAddress = instanceBufferAddr; // Address of instance data on GPU

        // Get TLAS build sizes
         VkAccelerationStructureBuildGeometryInfoKHR buildGeomInfo{};
         buildGeomInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
         buildGeomInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
         buildGeomInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
         if (allow_update) {
             buildGeomInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
         }
         buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; // Initial build
         buildGeomInfo.geometryCount = 1; // One geometry struct (pointing to instance buffer)
         buildGeomInfo.pGeometries = &tlasGeometry;

         uint32_t instanceCount = static_cast<uint32_t>(instances.size());
         VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
         buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
          // Call function directly (Volk ensures availability)
         vkGetAccelerationStructureBuildSizesKHR(g_Device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeomInfo, &instanceCount, &buildSizesInfo);


        // Create TLAS buffer and object
         Kinesis::Window::createBuffer(buildSizesInfo.accelerationStructureSize,
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    tlas.buffer, tlas.memory);

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = tlas.buffer;
        createInfo.size = buildSizesInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
         // Call function directly (Volk ensures availability)
        if (vkCreateAccelerationStructureKHR(g_Device, &createInfo, nullptr, &tlas.structure) != VK_SUCCESS) {
            delete_acceleration_structure(tlas); // Cleanup buffer/memory
            throw std::runtime_error("Failed to create TLAS!");
        }
        // Get TLAS address after creation
        VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo{};
        tlasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        tlasAddressInfo.accelerationStructure = tlas.structure;
        tlas.address = vkGetAccelerationStructureDeviceAddressKHR(g_Device, &tlasAddressInfo);


        // Create scratch buffer for TLAS build
        ScratchBuffer scratch = create_scratch_buffer(buildSizesInfo.buildScratchSize);

        // Build TLAS on GPU
        VkCommandBuffer cmdBufBuild = beginSingleTimeCommands();

        // Update buildGeomInfo for the build command
         buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
         buildGeomInfo.dstAccelerationStructure = tlas.structure; // Target TLAS object
         buildGeomInfo.scratchData.deviceAddress = scratch.address; // Scratch buffer

         // Define build range info for the instances
         VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
         buildRangeInfo.primitiveCount = instanceCount; // Number of instances in the buffer
         buildRangeInfo.primitiveOffset = 0; // Offset in the instance buffer
         buildRangeInfo.firstVertex = 0;     // Not used for instances
         buildRangeInfo.transformOffset = 0; // Not used for instances
         const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

         // Call function directly (Volk ensures availability)
        vkCmdBuildAccelerationStructuresKHR(cmdBufBuild, 1, &buildGeomInfo, &pBuildRangeInfo);

        // Barrier: Ensure TLAS build completes before scratch is deleted and before TLAS is used for tracing
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR; // Ready for shader read
        vkCmdPipelineBarrier(cmdBufBuild,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, // Source: Build stage
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, // Dest: Build (for scratch reuse) & RT Shader
                             0, 1, &barrier, 0, nullptr, 0, nullptr);


        endSingleTimeCommands(cmdBufBuild); // Submit and wait

        // Cleanup scratch buffer
        delete_scratch_buffer(scratch);

         std::cout << "TLAS created successfully with " << instanceCount << " instances." << std::endl;
    }


} // namespace Kinesis::RayTracerManager