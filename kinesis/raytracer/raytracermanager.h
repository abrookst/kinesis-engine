#ifndef RAYTRACINGMANAGER_H
#define RAYTRACINGMANAGER_H

#include "kinesis.h" // Include necessary base headers
#include <vector>

namespace Kinesis::RayTracingManager {
    struct AccelerationStructure {
        VkAccelerationStructureKHR structure;
        uint64_t address;
        VkBuffer buffer;
    };

    struct ScratchBuffer {
        uint64_t address;
        VkBuffer buffer;
        VkDeviceMemory memory;
    };

    extern VkDescriptorSetLayout rtDescriptorSetLayout;
    extern VkPipelineLayout rtPipelineLayout; // RT specific pipeline layout
    extern VkDescriptorSet rtDescriptorSet;
    // Add extern declarations for other RT resources if managed here
    // extern VkPipeline rtPipeline;
    // extern VkBuffer sbtBuffer;
    // extern VkDeviceMemory sbtBufferMemory;
    // ... etc ...
	extern VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rt_pipeline_properties{};
	extern VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{};
    extern std::vector<AccelerationStructure> blas;
	extern AccelerationStructure tlas;
    extern std::unique_ptr<VkBuffer> vertex_buffer;
    extern std::unique_ptr<VkBuffer> index_buffer;
    extern std::unique_ptr<VkBuffer> instances_buffer;
    extern std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups{};
    /*
    TODO: put the shader binding tables here
    */
    

    /**
     * @brief Initializes ray tracing resources if supported by hardware.
     * Creates descriptor set layouts, pipeline layouts, allocates sets, etc.
     * Should be called after core Vulkan setup (Window::initialize).
     */
    void initialize();

    /**
     * @brief Cleans up ray tracing resources if they were initialized.
     * Destroys layouts, frees sets (if needed), etc.
     * Should be called before core Vulkan cleanup (Window::cleanup).
     */
    void cleanup();

    /**
     * @brief Updates the descriptor set with current resource handles.
     * MUST be called only if ray tracing is available and after resources are ready.
     * @param tlas Handle to the Top-Level Acceleration Structure.
     * @param outputImgView Handle to the output storage image view.
     * @param camBuffer Handle to the camera uniform buffer.
     * @param camBufSize Size of the camera uniform buffer.
     * @param other resources... Pass other buffer/image handles as needed.
     */
    void updateDescriptorSet(VkAccelerationStructureKHR tlas, VkImageView outputImgView, VkBuffer camBuffer, VkDeviceSize camBufSize /*, other resources...*/);

    /**
     * @brief Binds the ray tracing pipeline and descriptor set.
     * MUST be called only if ray tracing is available.
     * @param commandBuffer The command buffer to bind to.
     */
    void bind(VkCommandBuffer commandBuffer);

    // --- Helper Functions (potentially static in .cpp or internal) ---
    // void createDescriptorSetLayout(); // Declaration if needed externally
    // void createPipelineLayout(); // Declaration if needed externally
    // void allocateDescriptorSet(); // Declaration if needed externally

    // Functions for the building and deletion of the acceleration structures
    /**
     * @brief a helper function to get the address of any buffer
     * @param buffer the buffer to get the address of
     * Need to check if any functions mirror this 
     */
	uint64_t getBufferAddress(VkBuffer buffer) const;

    /**
     * @brief 
     * @param size the size of the new buffer
     */
	ScratchBuffer create_scratch_buffer(VkDeviceSize size);

    /** 
     * @brief deletes and cleans up the memory used by the scratch buffer temporarily
     * @param scratch_buffer the buffer to delete
     */
	void delete_scratch_buffer(ScratchBuffer &scratch_buffer);

    /**
     * @brief creates the blas and stores it in the blas extern
     */
    void create_blas();

    /**
     * @brief creates the tlas and stores it in the tlas extern
     */
	void create_tlas();

    /**
     * @brief cleans up the memory of an acceleration structure
     */
	void delete_acceleration_structure(AccelerationStructure &acceleration_structure);

    protected:
    // just to assist with building both the blas and tlas
    VkTransformMatrixKHR accel_transform = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
} 

#endif 