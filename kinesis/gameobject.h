#ifndef __GAMEOBJECT_H__
#define __GAMEOBJECT_H__

#include <string>
#include "kinesis/transform.h"

class GameObject {
public:

private:
    std::string name;
    Transform transform;
};

#endif // __GAMEOBJECT_H__