#ifndef __TRIANGLE_H__
#define __TRIANGLE_H__

#include "mesh/edge.h"
#include "mesh/vertex.h"

namespace Kinesis::Raytracing {
	class Hit;
	class Ray;
}

namespace Kinesis::Mesh {
	class Material;
	class Triangle {
		public:
			// ========================
			// CONSTRUCTOR & DESTRUCTOR
			Triangle(Kinesis::Mesh::Material *m) {
				edge = nullptr;
				material = m;
			}

			// =========
			// ACCESSORS
			Kinesis::Mesh::Vertex* operator[](int i) const {
				assert (edge != nullptr);
				if (i == 0) return edge->getStartVertex();
				if (i == 1) return edge->getNext()->getStartVertex();
				if (i == 2) return edge->getNext()->getNext()->getStartVertex();
				assert(0); exit(0);
			}

			Kinesis::Mesh::Edge* getEdge() const {
				assert (edge != nullptr);
				return edge;
			}

			Kinesis::Mesh::Material* getMaterial() const { return material; }

			float getArea() const;
			//glm::vec3 randomPoint() const;
			glm::vec3 computeNormal() const;

			// =========
			// MODIFIERS
			void setEdge(Edge *e) {
				assert (edge == nullptr);
				assert (e != nullptr);
				edge = e;
			}

			// INTERSECTION
			bool intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h) const;

		protected:
			Triangle(const Kinesis::Mesh::Triangle &/*t*/) { assert(0); exit(0); }
			Kinesis::Mesh::Triangle& operator=(const Triangle &/*t*/) { assert(0); exit(0); }
			// ==============
			// REPRESENTATION
			Kinesis::Mesh::Edge *edge;
			Kinesis::Mesh::Material *material;

			// INTERSECTION PART 2
			bool plane_intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h) const;
			bool triangle_intersect(const Kinesis::Raytracing::Ray &r, Kinesis::Raytracing::Hit &h, Kinesis::Mesh::Vertex *a, Kinesis::Mesh::Vertex *b, Kinesis::Mesh::Vertex *c) const;
			double det3x3(double a1,double a2,double a3, double b1,double b2,double b3, double c1,double c2,double c3) const;
			double det2x2(double a, double b, double c, double d) const;

	};

}

#endif // __TRIANGLE_H__
