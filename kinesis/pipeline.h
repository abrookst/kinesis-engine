#ifndef PIPELINE_H
#define PIPELINE_H

#include "kinesis.h"

#include <string>
#include <vector>

namespace Kinesis::Pipeline
{
    struct ConfigInfo {
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    /**
     * @brief Initializes pipeline resources
     * @param vertFilePath Path to vertex shader file
     * @param fragFilePath Path to fragment shader file
     * @param configInfo Pipeline configuration
     */
    void initialize(const std::string& vertFilePath, const std::string& fragFilePath, ConfigInfo& configInfo);

    /**
     * @brief Cleans up pipeline resources
     */
    void cleanup();

    /**
     * @brief Creates default pipeline configuration
     * @param width Viewport width
     * @param height Viewport height
     * @return Default ConfigInfo structure
     */
    ConfigInfo defaultConfigInfo(uint32_t width, uint32_t height);

    /**
     * @brief Binds the pipeline to a command buffer
     * @param commandBuffer The command buffer to bind to
     */
    void bind(VkCommandBuffer commandBuffer);

    /**
     * @brief Reads a file into a byte vector
     * @param filePath Path to the file
     * @return Vector containing file contents
     */
    std::vector<char> readFile(const std::string& filePath);
}
#endif