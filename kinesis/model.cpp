#include "model.h"
#include <cassert>
#include <cstring>

namespace Kinesis {

    void Model::createVertexBuffers(const std::vector<Vertex> &vertices){
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3!" ); 
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;


        Kinesis::Window::createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            vertexBuffer,
            vertexBufferMemory 
        );


        void* data;
        vkMapMemory(g_Device, vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(g_Device, vertexBufferMemory);
    }

    void Model::bind(VkCommandBuffer commandBuffer){
        VkBuffer buffers[] = {vertexBuffer}; 
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    }

    void Model::draw(VkCommandBuffer commandBuffer){

        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0); 
    }

    Model::Model(const std::string &path, const std::string& input_file){
        mesh.Load(path, input_file);
        createVertexBuffers(mesh.getVertices());
    }

    Model::~Model(){
        vkDestroyBuffer(g_Device, vertexBuffer, nullptr);
        vkFreeMemory(g_Device, vertexBufferMemory, nullptr); 
    }

}