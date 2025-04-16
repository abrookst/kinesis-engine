#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include <string>
#include <memory>

namespace Kinesis {
    class Model; // Forward declare Model within its namespace
}

#include "transform.h"
#include "model.h"

class GameObject {
public:
    using id_t = unsigned int;

    static GameObject createGameObject(const std::string &name){
        static id_t currId = 0;
        return GameObject(currId++, name);
    }

    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;
    GameObject(GameObject &&) = default;
    GameObject &operator=(GameObject &&) = default;

    id_t getId() {return id; }

    std::shared_ptr<Kinesis::Model> model = nullptr;
    std::string name;
    Transform transform;
    glm::vec3 color;

private:
    GameObject(id_t objId, const std::string &name) : id(objId), name(name), transform(this), color(glm::vec3(1)) {}
    id_t id;
    
};

#endif // __GAMEOBJECT_H__
