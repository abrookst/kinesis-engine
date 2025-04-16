#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "math/vectors.h"
#include "math/matrix.h"

namespace Kinesis::Mesh {
	class Camera {
		public:
			// CONSTRUCTOR & DESTRUCTOR
			Camera(const Kinesis::Math::Vector3 &c, const Kinesis::Math::Vector3 &poi, const Kinesis::Math::Vector3 &u);
			virtual ~Camera() {}

			const Kinesis::Math::Matrix& getViewMatrix() const { return ViewMatrix; }
			const Kinesis::Math::Matrix& getProjectionMatrix() const { return ProjectionMatrix; }

		public:
			Camera() { assert(0); }
			Kinesis::Math::Vector3 point_of_interest;
			Kinesis::Math::Vector3 camera_position;
			Kinesis::Math::Vector3 up;
			Kinesis::Math::Matrix ViewMatrix;
			Kinesis::Math::Matrix ProjectionMatrix;
	};
}

#endif // __CAMERA_H__
