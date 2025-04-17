#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include <string>
#include <glm/glm.hpp>


class Transform {
public:
    // -----------------------------------------------
    // CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
    Transform(GameObject* gameObj = nullptr) : position(glm::vec3()),rotation(0),scale(glm::vec3(1)),gameObject(gameObj) {}
    Transform(const Transform &T, GameObject* gameObj = nullptr) {
        position = T.position;
        rotation = T.rotation;
        scale = T.scale;
        gameObject = gameObj;
    }
    Transform(const glm::vec3 &pos, const float &rot, const glm::vec3 &sc, GameObject* gameObj = nullptr) : position(pos),rotation(rot),scale(sc),gameObject(gameObj) {}
    const Transform &operator=(const Transform &T) {
        position = T.position;
        rotation = T.rotation;
        scale = T.scale;
        gameObject = nullptr;
        return *this;
    }
    
    // ----------------------------
    // SIMPLE ACCESSORS & MODIFIERS
    glm::vec3 Position() const { return position; }
    float Rotation() const { return rotation; }
    glm::vec3 Scale() const { return scale; }

    void SetPosition(const glm::vec3 &V) { position = V; }
    void Translate(const glm::vec3 &V) { position += V; }
    void SetRotation(const float &R) { rotation = R; }
    void Rotate(const float &R) { rotation *= R; }
    void SetScale(const glm::vec3 &V) { scale = V; }
    void ModifyScale(const glm::vec3 &V) { scale = scale * V; }

    glm::mat2 mat2(){
        const float rot_sin = glm::sin(rotation);
        const float rot_cos = glm::cos(rotation);

        glm::mat2 rotMTX{{rot_cos, rot_sin}, {-rot_sin, rot_cos}};
        glm::mat2 scalMTX{{scale.x, 0}, {0,scale.y}};
        return rotMTX * scalMTX;
    }
    
private:
    glm::vec3 position;
    float rotation;
    glm::vec3 scale;
    GameObject *gameObject;
};

			void SetPosition(const Math::Vector3 &V) { position = V; }
			void Translate(const Math::Vector3 &V) { position += V; }
			void SetRotation(const Math::Quaternion &Q) { rotation = Q; }
			void Rotate(const Math::Quaternion &Q) { rotation *= Q; }
			void SetScale(const Math::Vector3 &V) { scale = V; }
			void ModifyScale(const Math::Vector3 &V) { scale = scale * V; }
		private:
			Math::Vector3 position;
			Math::Quaternion rotation;
			Math::Vector3 scale;
			GameObject *gameObject;
	};

}
#endif // __TRANSFORM_H__
