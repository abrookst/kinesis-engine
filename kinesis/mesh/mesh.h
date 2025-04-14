#ifndef __MESH_H__
#define __MESH_H__

#include <vector>
#include "hash.h"
#include "material.h"

class Vertex;
class Edge;
class BoundingBox;
class Triangle;
class ArgParser;
class Ray;
class Hit;
class Camera;

class Mesh {
public:
	// ===============================
	// CONSTRUCTOR & DESTRUCTOR & LOAD
	Mesh() { bbox = NULL; }
	virtual ~Mesh();
	void Load(ArgParser *_args);

	// ========
	// VERTICES
	int numVertices() const { return vertices.size(); }
	Vertex* addVertex(const Vector3 &pos);
	// look up vertex by index from original .obj file
	Vertex* getVertex(int i) const {
		assert (i >= 0 && i < numVertices());
		return vertices[i];
	}
	// this creates a relationship between 3 vertices (2 parents, 1 child)
	void setParentsChild(Vertex *p1, Vertex *p2, Vertex *child);
	// this accessor will find a child vertex (if it exists) when given
	// two parent vertices
	Vertex* getChildVertex(Vertex *p1, Vertex *p2) const;

	// =====
	// EDGES
	int numEdges() const { return edges.size(); }
	// this efficiently looks for an edge with the given vertices, using a hash table
	Edge* getEdge(Vertex *a, Vertex *b) const;
	const edgeshashtype& getEdges() const { return edges; }

	// =================
	// ACCESS THE LIGHTS
	std::vector<Triangle*>& getLights() { return lights; }

	// ===============
	// OTHER ACCESSORS
	int numTriangles() const { return triangles.size(); }
	BoundingBox* getBoundingBox() const { return bbox; }

private:
	void addTriangle(Vertex *a, Vertex *b, Vertex *c, Material *material);
	void removeTriangleEdges(Triangle *t);

	// ==============
	// REPRESENTATION
	ArgParser *args;
public:
	std::vector<Material*> materials;
	Vector3 background_color;
	Camera *camera;
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

// ======================================================================
// ======================================================================

#endif // __MESH_H__
