#ifndef __VERTEX_H__
#define __VERTEX_H__

#include "glm/glm.hpp"

// ==========================================================

class Vertex {
	public:

		// ========================
		// CONSTRUCTOR & DESTRUCTOR
		Vertex(int i, const glm::vec3 &pos, const glm::vec3 &col) : position(pos), color(col) { index = i; s = 0; t = 0; }
		
		// =========
		// ACCESSORS
		int getIndex() const { return index; }
		const glm::vec3& getPos() const { return position; }
		const glm::vec3& getColor() const { return color; }
		float get_s() const { return s; }
		float get_t() const { return t; }

		// =========
		// MODIFIERS
		void setTextureCoordinates(float _s, float _t) { s = _s; t = _t; }

		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions(){
			return {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
		}
		
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(){
			 std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
			 attributeDescriptions[0].binding = 0; 
			 attributeDescriptions[0].location = 0; 
			 attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; 
			 attributeDescriptions[0].offset = offsetof(Vertex, position);
		
			 attributeDescriptions[1].binding = 0; 
			 attributeDescriptions[1].location = 1; 
			 attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; 
			 attributeDescriptions[1].offset = offsetof(Vertex, color);
		
			 return attributeDescriptions;
		}

	private:

		// ==============
		// REPRESENTATION
		glm::vec3 position;

		// texture coordinates
		// NOTE: arguably these should be stored at the faces of the mesh
		// rather than the vertices
		float s,t;

		// this is the index from the original .obj file.
		// technically not part of the half-edge data structure
		int index; 

		glm::vec3 color;
};

// ==========================================================

#endif // __VERTEX_H__

