#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Kinesis{
    class Transform {
        public:
            // -----------------------------------------------
            // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
            Transform() : translation(glm::vec3()),rotation(0),scale(glm::vec3(1)) {}
            Transform(const Transform &T) {
                translation = T.translation;
                rotation = T.rotation;
                scale = T.scale;
            }
            Transform(const glm::vec3 &trans, const float &rot, const glm::vec3 &sc) : translation(trans),rotation(rot),scale(sc) {}
            const Transform &operator=(const Transform &T) {
                translation = T.translation;
                rotation = T.rotation;
                scale = T.scale;
                return *this;
            }
            
            // ----------------------------
            // SIMPLE ACCESSORS & MODIFIERS
            glm::vec3 translation{};
            glm::vec3 rotation{};
            glm::vec3 scale{1.f,1.f,1.f};
        
            glm::mat4 mat4(){
                glm::mat4 transform = glm::translate(glm::mat4{1.f}, translation);
                transform = glm::rotate(transform, rotation.y, {0.f,1.f,0.f});
                transform = glm::rotate(transform, rotation.x, {1.f,0.f,0.f});
                transform = glm::rotate(transform, rotation.z, {0.f,0.f,1.f});
                transform = glm::scale(transform,scale);
                return transform;
            }
        };
}


#endif // __TRANSFORM_H__
