#ifndef RENDERSYSTEM_H
#define RENDERSYSTEM_H

#include "kinesis.h"

namespace Kinesis
{
    class RenderSystem
    {
    public:
        RenderSystem();
        ~RenderSystem();

        VkPipelineLayout pipelineLayout;
        void renderGameObjects(VkCommandBuffer commandBuffer);

        /**
         * @brief Creates the graphics pipeline.
         */
        void createPipeline();

        /**
         * @brief Creates the Vulkan pipeline layout.
         */
        void createPipelineLayout();
    };

    struct SimplePushConstantData
    {
        glm::mat2 transform{1.f};
        glm::vec2 offset;
        alignas(16) glm::vec3 color;
    };

}

#endif