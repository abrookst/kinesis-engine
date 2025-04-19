#include "keyboard_controller.h"

namespace Kinesis::Keyboard {

    KeyMappings keys;
    float moveSpeed{3.f};
    float lookspeed{1.5f};

    void moveInPlaneXZ(float dt, GameObject& gObj){
        glm::vec3 rotate{0.f};
        if(glfwGetKey(Kinesis::Window::window, keys.lookRight) == GLFW_PRESS){ rotate.y+=1.f; }
        if(glfwGetKey(Kinesis::Window::window, keys.lookLeft) == GLFW_PRESS){ rotate.y-=1.f; }
        if(glfwGetKey(Kinesis::Window::window, keys.lookUp) == GLFW_PRESS){ rotate.x+=1.f; }
        if(glfwGetKey(Kinesis::Window::window, keys.lookDown) == GLFW_PRESS){ rotate.x-=1.f; }
        //threw in the STD epsilon
        if(glm::dot(rotate,rotate) > std::numeric_limits<float>::epsilon()){
            gObj.transform.rotation += lookspeed * dt * glm::normalize(rotate);
        }
        
        gObj.transform.rotation.x = glm::clamp(gObj.transform.rotation.x, -1.5f, 1.5f);
        gObj.transform.rotation.y = glm::mod(gObj.transform.rotation.y, glm::two_pi<float>());

        float yaw = gObj.transform.rotation.y;
        const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
        const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
        const glm::vec3 upDir{0.f, -1.f, 0.f};

        glm::vec3 moveDir{0.f};
        if(glfwGetKey(Kinesis::Window::window, keys.moveForward) == GLFW_PRESS){ moveDir+=forwardDir; }
        if(glfwGetKey(Kinesis::Window::window, keys.moveBackward) == GLFW_PRESS){ moveDir-=forwardDir; }
        if(glfwGetKey(Kinesis::Window::window, keys.moveRight) == GLFW_PRESS){ moveDir+=rightDir; }
        if(glfwGetKey(Kinesis::Window::window, keys.moveLeft) == GLFW_PRESS){ moveDir-=rightDir; }
        if(glfwGetKey(Kinesis::Window::window, keys.moveUp) == GLFW_PRESS){ moveDir+=upDir; }
        if(glfwGetKey(Kinesis::Window::window, keys.moveDown) == GLFW_PRESS){ moveDir-=upDir; }
        if(glm::dot(moveDir,moveDir) > std::numeric_limits<float>::epsilon()){
            gObj.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }

}