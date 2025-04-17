#ifndef __VERTEX_H__
#define __VERTEX_H__

#include "glm/glm.hpp"

// ==========================================================

namespace Kinesis::Mesh {
	class Vertex {
		public:

		// ========================
		// CONSTRUCTOR & DESTRUCTOR
		Vertex(int i, const Vector3 &pos) : position(pos) { index = i; s = 0; t = 0; }

		// =========
		// ACCESSORS
		int getIndex() const { return index; }
		const Vector3& getPos() const { return position; }
		float get_s() const { return s; }
		float get_t() const { return t; }

			// =========
			// MODIFIERS
			void setTextureCoordinates(float _s, float _t) { s = _s; t = _t; }

	private:

		// ==============
		// REPRESENTATION
		Vector3 position;

			// texture coordinates
			// NOTE: arguably these should be stored at the faces of the mesh
			// rather than the vertices
			float s,t;

		// this is the index from the original .obj file.
		// technically not part of the half-edge data structure
		int index; 
};

}
// ==========================================================

#endif // __VERTEX_H__

