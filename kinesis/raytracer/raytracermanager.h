#ifndef RAYTRACINGMANAGER_H
#define RAYTRACINGMANAGER_H

#include "kinesis.h" // Include necessary base headers
#include <vector>

namespace Kinesis::RayTracingManager {
    extern VkDescriptorSetLayout rtDescriptorSetLayout;
    extern VkPipelineLayout rtPipelineLayout; // RT specific pipeline layout
    extern VkDescriptorSet rtDescriptorSet;
    // Add extern declarations for other RT resources if managed here
    // extern VkPipeline rtPipeline;
    // extern VkBuffer sbtBuffer;
    // extern VkDeviceMemory sbtBufferMemory;
    // ... etc ...

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

} 

#endif 