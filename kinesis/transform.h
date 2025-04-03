#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include <string>
#include "kinesis/math/vectors.h"
#include "kinesis/math/quaternion.h"

class GameObject;

class Transform {
public:
    // -----------------------------------------------
    // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
    Transform() : position(Vector3::Zero()),rotation(Quaternion::Identity()),scale(Vector3::One()),gameObject(nullptr) {}
    Transform(const Transform &T) {
        position = T.position;
        rotation = T.rotation;
        scale = T.scale;
        gameObject = nullptr;
    }
    Transform(const Vector3 &pos, const Quaternion &rot, const Vector3 &sc) : position(pos),rotation(rot),scale(sc),gameObject(nullptr) {}
    const Transform &operator=(const Transform &T) {
        position = T.position;
        rotation = T.rotation;
        scale = T.scale;
        gameObject = nullptr;
        return *this;
    }
    
    // ----------------------------
    // SIMPLE ACCESSORS & MODIFIERS
    Vector3 Position() const { return position; }
    Quaternion Rotation() const { return rotation; }
    Vector3 Scale() const { return scale; }

    void SetPosition(const Vector3 &V) { position = V; }
    void Translate(const Vector3 &V) { position += V; }
    void SetRotation(const Quaternion &Q) { rotation = Q; }
    void Rotate(const Quaternion &Q) { rotation *= Q; }
    void SetScale(const Vector3 &V) { scale = V; }
    void ModifyScale(const Vector3 &V) { scale = scale * V; }
private:
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
    GameObject *gameObject;
};

#endif // __TRANSFORM_H__