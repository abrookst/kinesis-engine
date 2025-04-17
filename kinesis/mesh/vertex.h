#ifndef __VERTEX_H__
#define __VERTEX_H__

#include "glm/glm.hpp"

// ==========================================================
namespace Kinesis::Mesh {
	class Vertex {
		public:
	
			// ========================
			// CONSTRUCTOR & DESTRUCTOR
			Vertex(int i, const glm::vec3 &pos, const glm::vec3 &col) : position(pos), color(col) { index = i; s = 0; t = 0; }
			
			glm::vec3 position;
			float s,t;
			int index; 
	
			glm::vec3 color;
	
	
			void setTextureCoordinates(float _s, float _t) { s = _s; t = _t; }
	
			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions(){
				return {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
			}
			
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(){
				 std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
				 attributeDescriptions[0].binding = 0; 
				 attributeDescriptions[0].location = 0; 
				 attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; 
				 attributeDescriptions[0].offset = offsetof(Vertex, position);
			
				 attributeDescriptions[1].binding = 0; 
				 attributeDescriptions[1].location = 1; 
				 attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; 
				 attributeDescriptions[1].offset = offsetof(Vertex, color);
			
				 return attributeDescriptions;
			}
	};
}


// ==========================================================

#endif // __VERTEX_H__

