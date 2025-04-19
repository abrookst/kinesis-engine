#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include "hash.h"
#include "material.h"

//class Camera;

namespace Kinesis::Mesh {
	class Vertex;
	class Edge;
	class BoundingBox;
	class Triangle;

	class Mesh {
		public:
			// ===============================
			// CONSTRUCTOR & DESTRUCTOR & LOAD
			Mesh() { bbox = NULL; }
			~Mesh();
			void Load(const std::string &path, const std::string& input_file);

			// ========
			// VERTICES
			size_t numVertices() const { return vertices.size(); }
			Vertex* addVertex(const glm::vec3 &pos);
			// look up vertex by index from original .obj file
			Vertex* getVertex(int i) const {
				assert (i >= 0 && i < numVertices());
				return vertices[i];
			}
			std::vector<Vertex> getVertices() { 
				std::vector<Vertex> verticesCopy;
				for (int i = 0; i < numVertices(); i++) {
					verticesCopy.push_back(*vertices[i]);
				}
				return verticesCopy;
			 }
			// this creates a relationship between 3 vertices (2 parents, 1 child)
			void setParentsChild(Vertex *p1, Vertex *p2, Vertex *child);
			// this accessor will find a child vertex (if it exists) when given
			// two parent vertices
			Vertex* getChildVertex(Vertex *p1, Vertex *p2) const;

			// =====
			// EDGES
			size_t numEdges() const { return edges.size(); }
			// this efficiently looks for an edge with the given vertices, using a hash table
			Edge* getEdge(Vertex *a, Vertex *b) const;
			const edgeshashtype& getEdges() const { return edges; }

			// =================
			// ACCESS THE LIGHTS
			std::vector<Triangle*>& getLights() { return lights; }

			// ===============
			// OTHER ACCESSORS
			size_t numTriangles() const { return triangles.size(); }
			BoundingBox* getBoundingBox() const { return bbox; }

		private:
			void addTriangle(Vertex *a, Vertex *b, Vertex *c, Material *material);
			void removeTriangleEdges(Triangle *t);

			// ==============
			// REPRESENTATION
		public:
			std::vector<Material*> materials;
			glm::vec3 background_color;
		private:
			// the bounding box of all rasterized faces in the scene
			BoundingBox *bbox;

			// the vertices & edges used by all quads (including rasterized primitives)
			std::vector<Vertex*> vertices;
			edgeshashtype edges;
			vphashtype vertex_parents;

			// the quads from the .obj file (before subdivision)
			std::vector<Triangle*> triangles;
			// the quads from the .obj file that have non-zero emission value
			std::vector<Triangle*> lights; 
	};

}
// ======================================================================
// ======================================================================

#endif // __MESH_H__
