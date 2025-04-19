#ifndef __EDGE_H__
#define __EDGE_H__

#include <cassert>
#include <cstdlib>

#include "mesh/vertex.h"

// ===================================================================
// half-edge data structure

namespace Kinesis::Mesh {
	class Triangle;

	class Edge {
		public:

			// ========================
			// CONSTRUCTORS & DESTRUCTOR
			Edge(Vertex *vs, Vertex *ve, Triangle *t) : start_vertex(vs), end_vertex(ve), triangle(t), next(nullptr), opposite(nullptr) {}
			~Edge() { if (opposite != nullptr) { opposite->opposite = NULL; } };

			// =========
			// ACCESSORS
			Vertex* getStartVertex() const { assert (start_vertex != NULL); return start_vertex; }
			Vertex* getEndVertex() const { assert (end_vertex != NULL); return end_vertex; }
			Edge* getNext() const { assert (next != NULL); return next; }
			Triangle* getTriangle() const { assert (triangle != NULL); return triangle; }
			Edge* getOpposite() const {
				// warning!  the opposite edge might be NULL!
				return opposite; }
			glm::length_t Length() const { return (start_vertex->position - end_vertex->position).length(); };

			// =========
			// MODIFIERS
			void setOpposite(Edge *e) {
				assert (opposite == NULL); 
				assert (e != NULL);
				assert (e->opposite == NULL);
				opposite = e; 
				e->opposite = this; 
			}
			void clearOpposite() { 
				if (opposite == NULL) return; 
				assert (opposite->opposite == this); 
				opposite->opposite = NULL;
				opposite = NULL; 
			}
			void setNext(Edge *e) {
				assert (next == NULL);
				assert (e != NULL);
				assert (triangle == e->triangle);
				next = e;
			}

		private:

			Edge(const Edge&) { assert(0); }
			Edge& operator=(const Edge&) { assert(0); exit(0); }

			// ==============
			// REPRESENTATION
			// in the half edge data adjacency data structure, the edge stores everything!
			// note: it's technically not necessary to store both vertices, but it makes
			//   dealing with non-closed meshes easier
			Vertex *start_vertex;
			Vertex *end_vertex;
			Triangle *triangle;
			Edge *opposite;
			Edge *next;
	};

}
// ===================================================================

#endif // __EDGE_H__

