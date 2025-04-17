#ifndef RENDERSYSTEM_H
#define RENDERSYSTEM_H

#include "kinesis.h"
#include "camera.h"

namespace Kinesis
{
    class RenderSystem
    {
    public:
        RenderSystem();
        ~RenderSystem();

        VkPipelineLayout pipelineLayout;
        void renderGameObjects(VkCommandBuffer commandBuffer, const Camera& camera);

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
        glm::mat4 transform{1.f};
        alignas(16) glm::vec3 color;
    };

}

#endif