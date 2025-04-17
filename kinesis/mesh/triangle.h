#ifndef __TRIANGLE_H__
#define __TRIANGLE_H__

#include "mesh/edge.h"
#include "mesh/vertex.h"


namespace Kinesis::Mesh {
	class Material;
	class Triangle {
		public:
			// ========================
			// CONSTRUCTOR & DESTRUCTOR
			Triangle(Material *m) {
				edge = nullptr;
				material = m;
			}

			// =========
			// ACCESSORS
			Vertex* operator[](int i) const {
				assert (edge != nullptr);
				if (i == 0) return edge->getStartVertex();
				if (i == 1) return edge->getNext()->getStartVertex();
				if (i == 2) return edge->getNext()->getNext()->getStartVertex();
				assert(0); exit(0);
			}

			Edge* getEdge() const {
				assert (edge != nullptr);
				return edge;
			}

			Material* getMaterial() const { return material; }

			float getArea() const;
			glm::vec3 randomPoint() const;
			glm::vec3 computeNormal() const;

			// =========
			// MODIFIERS
			void setEdge(Edge *e) {
				assert (edge == nullptr);
				assert (e != nullptr);
				edge = e;
			}
		protected:
			Triangle(const Triangle &/*t*/) { assert(0); exit(0); }
			Triangle& operator=(const Triangle &/*t*/) { assert(0); exit(0); }
			// ==============
			// REPRESENTATION
			Edge *edge;
			Material *material;
	};

}

#endif // __TRIANGLE_H__
