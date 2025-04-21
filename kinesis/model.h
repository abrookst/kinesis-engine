#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include "window.h"
#include "mesh/mesh.h"
#include "mesh/vertex.h"

#define GLM_FORCE_RADIANS           // Ensure GLM uses radians
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Vulkan depth range is [0, 1]
#include <glm/glm.hpp>

namespace Kinesis
{
    class Model
    {
        // --- Module Variables ---
    private:
        Mesh::Mesh mesh;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory; // Use extern
        uint32_t vertexCount;
        void createVertexBuffers(const std::vector<Mesh::Vertex> &vertices);

 
    public:
        /**
         * @brief returns the address of the attached mesh
         */
        Mesh::Mesh *getMesh() { return &mesh; }

        /**
         * @brief returns the vertex buffer
         */
        VkBuffer getVertexBuffer() { return vertexBuffer; }

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
        Model(const std::vector<Mesh::Vertex> &vertices);

        /**
         * @brief Initializes the model from an .obj file.
         * (Currently just calls createVertexBuffers).
         * @param path The folder containing the file.
         * @param input_file The name of the .obj file.
         */
        Model(const std::string &path, const std::string &input_file);

        /**
         * @brief Cleans up Vulkan resources (vertex buffer and memory) used by the model.
         */
        ~Model();
    };

}

#endif // MODEL_H