#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include <string>
#include "transform.h"

class GameObject {
public:
    GameObject(const std::string &name) : name(name), transform(this) {}

    const std::string& GetName() const { return name; }
    Transform& GetTransform() { return transform; }

    void SetName(const std::string &newName) { name = newName; }
    void SetTransform(const Transform &newTransform) { transform = newTransform; }
private:
    std::string name;
    Transform transform;
};

#endif // __GAMEOBJECT_H__
