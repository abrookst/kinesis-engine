#include <iostream>
#include <fstream>
#include <assert.h>
#include <string>
#include <utility>

// #include "argparser.h"
#include "mesh/vertex.h"
#include "mesh/boundingbox.h"
#include "mesh/mesh.h"
#include "mesh/edge.h"
#include "mesh/triangle.h"
#include "mesh/material.h"
#include "hash.h"
// #include "ray.h"
// #include "hit.h"
// #include "camera.h"

namespace Kinesis::Mesh {

	// =======================================================================
	// DESTRUCTOR
	// =======================================================================

	Mesh::~Mesh() {
		unsigned int i;
		for (i = 0; i < triangles.size(); i++) {
			Triangle *t = triangles[i];
			removeTriangleEdges(t);
			delete t;
		}
		for (i = 0; i < materials.size(); i++) { delete materials[i]; }
		for (i = 0; i < vertices.size(); i++) { delete vertices[i]; }
		delete bbox;
	}

	// =======================================================================
	// MODIFIERS:   ADD & REMOVE
	// =======================================================================

	Vertex* Mesh::addVertex(const glm::vec3 &position) {
		size_t index = numVertices();
		vertices.push_back(new Vertex((int)index,position));
		// extend the bounding box to include this point
		if (bbox == NULL) 
			bbox = new BoundingBox(position,position);
		else 
			bbox->Extend(position);
		return vertices[index];
	}

	void Mesh::addTriangle(Vertex *a, Vertex *b, Vertex *c, Material *material) {
		// create the face
		Triangle *t = new Triangle(material);
		// create the edges
		Edge *ea = new Edge(a,b,t);
		Edge *eb = new Edge(b,c,t);
		Edge *ec = new Edge(c,a,t);
		// point the face to one of its edges
		t->setEdge(ea);
		// connect the edges to each other
		ea->setNext(eb);
		eb->setNext(ec);
		ec->setNext(ea);
		// verify these edges aren't already in the mesh 
		// (which would be a bug, or a non-manifold mesh)
		assert (edges.find(std::make_pair(a,b)) == edges.end());
		assert (edges.find(std::make_pair(b,c)) == edges.end());
		assert (edges.find(std::make_pair(c,a)) == edges.end());
		// add the edges to the master list
		edges[std::make_pair(a,b)] = ea;
		edges[std::make_pair(b,c)] = eb;
		edges[std::make_pair(c,a)] = ec;
		// connect up with opposite edges (if they exist)
		edgeshashtype::iterator ea_op = edges.find(std::make_pair(b,a)); 
		edgeshashtype::iterator eb_op = edges.find(std::make_pair(c,b)); 
		edgeshashtype::iterator ec_op = edges.find(std::make_pair(a,c)); 
		if (ea_op != edges.end()) { ea_op->second->setOpposite(ea); }
		if (eb_op != edges.end()) { eb_op->second->setOpposite(eb); }
		if (ec_op != edges.end()) { ec_op->second->setOpposite(ec); }

		triangles.push_back(t);

		// if it's a light, add it to that list too
		//if ((material->getEmittedColor()).Length() > 0) {
		//	triangles.push_back(t);
		//}
	}

	void Mesh::removeTriangleEdges(Triangle *t) {
		// helper function for face deletion
		Edge *ea = t->getEdge();
		Edge *eb = ea->getNext();
		Edge *ec = eb->getNext();
		assert (ec->getNext() == ea);
		Vertex *a = ea->getStartVertex();
		Vertex *b = eb->getStartVertex();
		Vertex *c = ec->getStartVertex();
		// remove elements from master lists
		edges.erase(std::make_pair(a,b)); 
		edges.erase(std::make_pair(b,c)); 
		edges.erase(std::make_pair(c,a)); 
		// clean up memory
		delete ea;
		delete eb;
		delete ec;
	}

	std::vector<Vertex> Mesh::getFaceVertices() { 
		std::vector<Vertex> verticesCopy;
		for (int i = 0; i < triangles.size(); i++) {
			verticesCopy.push_back(*(*triangles[i])[0]);
			verticesCopy.push_back(*(*triangles[i])[1]);
			verticesCopy.push_back(*(*triangles[i])[2]);
		}
		return verticesCopy;
	}
	// ==============================================================================
	// EDGE HELPER FUNCTIONS

	Edge* Mesh::getEdge(Vertex *a, Vertex *b) const {
		edgeshashtype::const_iterator iter = edges.find(std::make_pair(a,b));
		if (iter == edges.end()) return NULL;
		return iter->second;
	}

	Vertex* Mesh::getChildVertex(Vertex *p1, Vertex *p2) const {
		vphashtype::const_iterator iter = vertex_parents.find(std::make_pair(p1,p2)); 
		if (iter == vertex_parents.end()) return NULL;
		return iter->second; 
	}

	void Mesh::setParentsChild(Vertex *p1, Vertex *p2, Vertex *child) {
		assert (vertex_parents.find(std::make_pair(p1,p2)) == vertex_parents.end());
		vertex_parents[std::make_pair(p1,p2)] = child; 
	}

	//
	// ===============================================================================
	// the load function parses our (non-standard) extension of very simple .obj files
	// ===============================================================================

	void Mesh::Load(const std::string &path, const std::string& input_file) {

		std::string file = path+'/'+input_file;

		std::ifstream objfile(file.c_str());
		if (!objfile.good()) {
			std::cout << "ERROR! CANNOT OPEN " << file << std::endl;
			return;
		}

		std::string token;
		Material *active_material = NULL;
		background_color = glm::vec3(1,1,1);

		while (objfile >> token) {
			if (token == "v") {
				float x,y,z;
				objfile >> x >> y >> z;
				addVertex(glm::vec3(x,y,z));
			} else if (token == "vt") {
				assert (numVertices() >= 1);
				float s,t;
				objfile >> s >> t;
				getVertex((int)numVertices()-1)->setTextureCoordinates(s,t);
			} else if (token == "f") {
				int a,b,c;
				objfile >> a >> b >> c;
				a--;
				b--;
				c--;
				assert (a >= 0 && a < numVertices());
				assert (b >= 0 && b < numVertices());
				assert (c >= 0 && c < numVertices());
				assert (active_material != NULL);
				addTriangle(getVertex(a),getVertex(b),getVertex(c),active_material);
			} else if (token == "background_color") {
				float r,g,b;
				objfile >> r >> g >> b;
				background_color = glm::vec3(r,g,b);
			} else if (token == "m") {
				// this is not standard .obj format!!
				// materials
				int m;
				objfile >> m;
				assert (m >= 0 && m < (int)materials.size());
				active_material = materials[m];
			} else if (token == "material") {
				// this is not standard .obj format!!
				std::string texture_file = "";
				glm::vec3 diffuse(0,0,0);
				float r,g,b;
				objfile >> token;
				if (token == "diffuse") {
					objfile >> r >> g >> b;
					diffuse = glm::vec3(r,g,b);
				} else {
					assert (token == "texture_file");
					objfile >> texture_file;
					// prepend the directory name
					texture_file = path+'/'+texture_file;
				}
				glm::vec3 reflective,transmissive,emitted;
				objfile >> token >> r >> g >> b;
				assert (token == "reflective");
				reflective = glm::vec3(r,g,b);
				float roughness = 0;
				objfile >> token;
				if (token == "roughness") {
					objfile >> roughness;
					objfile >> token;
				}
				double indexOfRefraction = 1;
				if (token == "transmissive") {
					objfile >> r >> g >> b >> token;
					transmissive = glm::vec3(r,g,b);
					if (token == "index") {
						objfile >> indexOfRefraction >> token;
					}
				}
				assert (token == "emitted");
				objfile >> r >> g >> b;
				emitted = glm::vec3(r,g,b);
				materials.push_back(new Kinesis::Mesh::Material(texture_file,diffuse,reflective,transmissive,emitted,roughness,indexOfRefraction));
			} else {
				std::cout << "UNKNOWN TOKEN " << token << std::endl;
				exit(0);
			}
		}
		std::cout << " mesh loaded: " << numTriangles() << " triangles and " << numEdges() << " edges." << std::endl;
	}
}

