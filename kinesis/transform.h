#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include "math/vectors.h"
#include "math/quaternion.h"


namespace Kinesis {
	class GameObject;
	class Transform {
		public:
			// -----------------------------------------------
			// CONSTRUCTORS, ASSIGNMENT OPERATOR, & DESTRUCTOR
			Transform(GameObject* gameObj = nullptr) : position(Math::Vector3::Zero()),rotation(Math::Quaternion::Identity()),scale(Math::Vector3::One()),gameObject(gameObj) {}
			Transform(const Transform &T, GameObject* gameObj = nullptr) {
				position = T.position;
				rotation = T.rotation;
				scale = T.scale;
				gameObject = gameObj;
			}
			Transform(const Math::Vector3 &pos, const Math::Quaternion &rot, const Math::Vector3 &sc, GameObject* gameObj = nullptr) : position(pos),rotation(rot),scale(sc),gameObject(gameObj) {}
			const Transform &operator=(const Transform &T) {
				position = T.position;
				rotation = T.rotation;
				scale = T.scale;
				gameObject = nullptr;
				return *this;
			}

			// ----------------------------
			// SIMPLE ACCESSORS & MODIFIERS
			Math::Vector3 Position() const { return position; }
			Math::Quaternion Rotation() const { return rotation; }
			Math::Vector3 Scale() const { return scale; }

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
