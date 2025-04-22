#ifndef __VERTEX_H__
#define __VERTEX_H__

#include <vector>
#include <vulkan/vulkan.h>
#include "glm/glm.hpp"

// ==========================================================
namespace Kinesis::Mesh {
	class Vertex {
		public:

			// ========================
			// CONSTRUCTOR & DESTRUCTOR
            // <<< Add normal and texCoord to constructor if loading them >>>
			Vertex(int i, const glm::vec3 &pos, const glm::vec3 &col = glm::vec3(0,0,0), const glm::vec3 &norm = glm::vec3(0,1,0), const glm::vec2 &uv = glm::vec2(0,0))
             : index(i), position(pos), color(col), normal(norm), texCoord(uv) {} // Initialize new members

			glm::vec3 position;
            glm::vec3 color;    // Base color or tint
            glm::vec3 normal;   // Vertex normal
            glm::vec2 texCoord; // Texture Coordinate

			// float s,t; // Redundant if using texCoord
			int index;


			// void setTextureCoordinates(float _s, float _t) { texCoord = {_s, _t}; } // Use texCoord directly

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions(){
				// One binding for all vertex attributes
                return {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
			}

			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(){
                // <<< Updated to include normal and texCoord >>>
				 std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4); // Now 4 attributes

                 // Position
				 attributeDescriptions[0].binding = 0;
				 attributeDescriptions[0].location = 0; // Corresponds to layout(location=0) in VS
				 attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				 attributeDescriptions[0].offset = offsetof(Vertex, position);

                 // Color
				 attributeDescriptions[1].binding = 0;
				 attributeDescriptions[1].location = 1; // Corresponds to layout(location=1) in VS
				 attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
				 attributeDescriptions[1].offset = offsetof(Vertex, color);

                 // Normal <<< NEW >>>
                 attributeDescriptions[2].binding = 0;
                 attributeDescriptions[2].location = 2; // Corresponds to layout(location=2) in VS
                 attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
                 attributeDescriptions[2].offset = offsetof(Vertex, normal);

                 // TexCoord <<< NEW >>>
                 attributeDescriptions[3].binding = 0;
                 attributeDescriptions[3].location = 3; // Corresponds to layout(location=3) in VS
                 attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT; // vec2 format
                 attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

				 return attributeDescriptions;
			}

            // Add comparison operator if needed for std::vector/map etc.
            bool operator==(const Vertex& other) const {
                return position == other.position && color == other.color && normal == other.normal && texCoord == other.texCoord;
            }
	};
}


// ==========================================================

#endif // __VERTEX_H__