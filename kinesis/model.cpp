#include <cassert>
#include <cstring>
#include <stdexcept> // Include for std::runtime_error
#include "model.h"
#include "mesh/mesh.h"
#include "window.h" // Include for Kinesis::Window::createBuffer
#include <iostream>

namespace Kinesis {

    void Model::createVertexBuffers(const std::vector<Mesh::Vertex> &vertices){
        vertexCount = static_cast<uint32_t>(vertices.size());
        // Allow vertexCount to be 0 if the mesh load failed or was empty
        if (vertexCount == 0) {
             vertexBuffer = VK_NULL_HANDLE;
             vertexBufferMemory = VK_NULL_HANDLE;
             // std::cerr << "Warning: Creating vertex buffer with 0 vertices." << std::endl; // Optional warning
            return;
        }
        // assert(vertexCount >= 3 && "Vertex count must be at least 3!" ); // Removing assert, allow models with <3 vertices initially
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;


        // Use the helper from window.cpp
        Kinesis::Window::createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, // Add RT usage flags
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vertexBuffer,
            vertexBufferMemory
        );


        void* data;
        // Check if buffer creation was successful before mapping
        if (vertexBuffer != VK_NULL_HANDLE && vertexBufferMemory != VK_NULL_HANDLE) {
            vkMapMemory(g_Device, vertexBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
            vkUnmapMemory(g_Device, vertexBufferMemory);
        } else if (vertexCount > 0) {
            // Throw error only if vertices existed but buffer creation failed
             throw std::runtime_error("Failed to create vertex buffer even though vertex data exists!");
        }
    }

    void Model::createIndexBuffers(const std::vector<uint32_t> &indices){
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;
        if(!hasIndexBuffer){
             indexBuffer = VK_NULL_HANDLE;
             indexBufferMemory = VK_NULL_HANDLE;
            return;
        }
        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

        // Use the helper from window.cpp
        Kinesis::Window::createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, // Add RT usage flags
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            indexBuffer,
            indexBufferMemory
        );


        void* data;
         // Check if buffer creation was successful before mapping
         if (indexBuffer != VK_NULL_HANDLE && indexBufferMemory != VK_NULL_HANDLE) {
            vkMapMemory(g_Device, indexBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
            vkUnmapMemory(g_Device, indexBufferMemory);
        } else {
             throw std::runtime_error("Failed to create index buffer even though index data exists!");
        }
    }

    void Model::bind(VkCommandBuffer commandBuffer){
        if (vertexBuffer != VK_NULL_HANDLE) { // Only bind if buffer exists
            VkBuffer buffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        }
        if(hasIndexBuffer && indexBuffer != VK_NULL_HANDLE){ // Only bind if index buffer exists
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void Model::draw(VkCommandBuffer commandBuffer){
         // Only draw if vertex buffer exists
         if (vertexBuffer == VK_NULL_HANDLE || vertexCount == 0) {
             return;
         }

        if(hasIndexBuffer && indexBuffer != VK_NULL_HANDLE){ // Check index buffer handle too
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        } else if (!hasIndexBuffer) { // Only draw non-indexed if no index buffer was intended
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
        // If hasIndexBuffer is true but indexBuffer is VK_NULL_HANDLE, it indicates an error state - don't draw.
    }

    Model::Model(const Builder &builder){
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
    }

    // Constructor loading from file
    Model::Model(const std::string &path, const std::string& input_file){
        // Combine path and file name correctly
        std::string fullPath = path + "/" + input_file;
        if (!mesh.Load(fullPath)) { // Use the single-argument Load
             // Handle error: Maybe throw an exception or set model to an error state
             std::cerr << "Error loading mesh: " << fullPath << std::endl;
             // Ensure buffers are null if loading fails
             vertexBuffer = VK_NULL_HANDLE;
             vertexBufferMemory = VK_NULL_HANDLE;
             indexBuffer = VK_NULL_HANDLE;
             indexBufferMemory = VK_NULL_HANDLE;
             vertexCount = 0;
             indexCount = 0;
             hasIndexBuffer = false;
             // Optionally re-throw or return an error indicator
             throw std::runtime_error("Failed to load model: " + fullPath);
        }
        // Use the correct accessor from the simplified mesh
        createVertexBuffers(mesh.getVertices());
        createIndexBuffers(mesh.getIndices()); // Create index buffers using loaded indices
    }

    Model::~Model(){
        // Check device handle validity from kinesis.h
        if (g_Device != VK_NULL_HANDLE) {
            if (vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(g_Device, vertexBuffer, nullptr);
            }
            if (vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(g_Device, vertexBufferMemory, nullptr);
            }
            if(hasIndexBuffer){
                 if (indexBuffer != VK_NULL_HANDLE) {
                     vkDestroyBuffer(g_Device, indexBuffer, nullptr);
                 }
                 if (indexBufferMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(g_Device, indexBufferMemory, nullptr);
                 }
            }
        }
         // Nullify handles after destruction (good practice)
         vertexBuffer = VK_NULL_HANDLE;
         vertexBufferMemory = VK_NULL_HANDLE;
         indexBuffer = VK_NULL_HANDLE;
         indexBufferMemory = VK_NULL_HANDLE;
    }

} // namespace Kinesis