#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include "window.h" 

#define GLM_FORCE_RADIANS           // Ensure GLM uses radians
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Vulkan depth range is [0, 1]
#include <glm/glm.hpp> 

namespace Kinesis::Model
{
    // --- Module Variables ---

    /**
     * @brief Handle to the Vulkan buffer storing vertex data.
     */
    extern VkBuffer vertexBuffer; // Use extern since definition is in .cpp

    /**
     * @brief Handle to the Vulkan device memory allocated for the vertex buffer.
     */
    extern VkDeviceMemory vertexBufferMemory; // Use extern

    /**
     * @brief The number of vertices in the vertex buffer.
     */
    extern uint32_t vertexCount; // Use extern

    // --- Vertex Structure ---

    /**
     * @brief Represents a single vertex with position data.
     * Can be extended to include color, normals, texture coordinates, etc.
     */
    struct Vertex
    {
        glm::vec2 position;
        glm::vec3 color;

        /**
         * @brief Provides the binding description for the Vertex structure.
         * Tells Vulkan how vertex data is laid out per binding point.
         * @return A vector containing the VkVertexInputBindingDescription.
         */
        static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();

        /**
         * @brief Provides the attribute descriptions for the Vertex structure.
         * Tells Vulkan how each attribute (e.g., position, color) within a vertex is formatted.
         * @return A vector containing the VkVertexInputAttributeDescription(s).
         */
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    // --- Public Functions ---

    /**
     * @brief Creates the Vulkan vertex buffer and allocates memory for it.
     * Copies the provided vertex data into the buffer.
     * @param vertices A vector containing the vertex data to upload.
     */
    void createVertexBuffers(const std::vector<Vertex> &vertices);

    /**
     * @brief Binds the vertex buffer to the specified command buffer for drawing.
     * @param commandBuffer The command buffer to bind the vertex buffer to.
     */
    void bind(VkCommandBuffer commandBuffer);

    /**
     * @brief Records a draw command into the specified command buffer using the bound vertex buffer.
     * @param commandBuffer The command buffer to record the draw command into.
     */
    void draw(VkCommandBuffer commandBuffer);

    /**
     * @brief Initializes the model by creating the vertex buffer with the given data.
     * (Currently just calls createVertexBuffers).
     * @param vertices The vertex data to initialize the model with.
     */
    void initialize(const std::vector<Vertex> &vertices);

    /**
     * @brief Cleans up Vulkan resources (vertex buffer and memory) used by the model.
     */
    void cleanup();

} // namespace Kinesis::Model

#endif // MODEL_H