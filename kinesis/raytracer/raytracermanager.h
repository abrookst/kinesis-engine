#ifndef RAYTRACERMANAGER_H
#define RAYTRACINMANAGER_H

#include "kinesis.h" // Include necessary base headers
#include <vector>
#include <memory> // For std::unique_ptr

namespace Kinesis::RayTracerManager {
    // --- Existing Structs ---
    struct AccelerationStructure {
        VkAccelerationStructureKHR structure = VK_NULL_HANDLE; // Initialize
        uint64_t address = 0;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE; // Add memory handle
    };

    struct ScratchBuffer {
        uint64_t address = 0;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    // --- Shader Binding Table Entry ---
    struct ShaderBindingTableEntry {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkStridedDeviceAddressRegionKHR addressRegion{};
    };

    struct RTOutput {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format for reflections
    };


    // --- Existing Extern Declarations ---
    extern VkDescriptorSetLayout rtDescriptorSetLayout;
    extern VkPipelineLayout rtPipelineLayout; // RT specific pipeline layout
    extern VkDescriptorSet rtDescriptorSet;
    extern VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties; // Renamed from rtPipelineProperties
    extern VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features;
    extern std::vector<AccelerationStructure> blas;
	extern AccelerationStructure tlas;
    // Remove vertex/index buffer externs if managed elsewhere (e.g., Model)
    // extern std::unique_ptr<VkBuffer> vertex_buffer;
    // extern std::unique_ptr<VkBuffer> index_buffer;
    extern VkBuffer instances_buffer; // Keep instance buffer if managed here
    extern VkDeviceMemory instances_buffer_memory; // Add memory for instance buffer
    extern std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

    // --- NEW Extern Declarations for RT Pipeline and SBT ---
    extern VkPipeline rtPipeline; // The ray tracing pipeline object
    extern ShaderBindingTableEntry rgenSBT; // RayGen SBT entry
    extern ShaderBindingTableEntry missSBT; // Miss SBT entry
    extern ShaderBindingTableEntry chitSBT; // ClosestHit SBT entry
    extern RTOutput rtOutput;
    // Potentially add ahitSBT if using AnyHit shaders

    // --- Existing Function Declarations ---
    void initialize(VkExtent2D extent);
    void cleanup();
    void allocateAndUpdateRtDescriptorSet(VkAccelerationStructureKHR tlasHandle, VkBuffer camBuffer, VkDeviceSize camBufSize);
    void updateDescriptorSet(VkAccelerationStructureKHR tlasHandle, VkImageView outputImgView, VkBuffer camBuffer, VkDeviceSize camBufSize /*, other resources...*/);
    void bind(VkCommandBuffer commandBuffer); // Should bind pipeline AND descriptor set
    uint64_t getBufferDeviceAddress(VkBuffer buffer); // Make public if needed outside
    ScratchBuffer create_scratch_buffer(VkDeviceSize size);
	void delete_scratch_buffer(ScratchBuffer &scratch_buffer);
    void create_blas();
	void create_tlas(bool allow_update = false); // Default allow_update to false
	void delete_acceleration_structure(AccelerationStructure &acceleration_structure);
    void updateGbufferDescriptors();

    void createRayTracingPipeline();
    void createShaderBindingTable();
    VkShaderModule createShaderModule(const std::string& filePath);
    void createRtOutputImage(VkExtent2D extent); // Add declaration
    void destroyRtOutputImage(); // Add declaration
    void createRtDescriptorSetLayout(); // Add declaration

    void traceRays(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height);

    inline static const VkTransformMatrixKHR accel_transform = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
}

#endif // RayTracerManager_H