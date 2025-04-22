#ifndef CAMERA_H
#define CAMERA_H

#define GLM_FORCE_RADIANS           // Ensure GLM uses radians
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Vulkan depth range is [0, 1]
#include <glm/glm.hpp>

namespace Kinesis {
    class Camera {
        public:
        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        void setPerspectiveProjection(float fovY, float aspect, float near, float far);

        void setViewDirection(glm::vec3 pos, glm::vec3 dir, glm::vec3 up = glm::vec3{0.f,1.f,0.f});
        void setViewTarget(glm::vec3 pos, glm::vec3 targ, glm::vec3 up = glm::vec3{0.f,1.f,0.f});
        void setViewYXZ(glm::vec3 pos, glm::vec3 rotation);

        const glm::mat4& getProjection() const {return projectionMatrix;}
        const glm::mat4& getView() const {return viewMatrix;}

        private:
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
    };
}

#endif