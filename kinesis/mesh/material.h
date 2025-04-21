#ifndef __MATERIAL_H__
#define __MATERIAL_H__

#include <string>
#include "glm/glm.hpp"

namespace Kinesis::Raytracing {
	class Hit;
	class Ray;
}

namespace Kinesis::Mesh {
	class Material {
		public:		
		Material(const std::string &texture_file, const glm::vec3 &d_color,
			const glm::vec3 &r_color, const glm::vec3 &t_color, const glm::vec3 &e_color, 
			float roughness_, float indexOfRefraction_) {
		  textureFile = texture_file;
		  if (textureFile != "") {
			//image = new Image(textureFile);
			//ComputeAverageTextureColor();
		  } else {
			diffuseColor = d_color;
			//image = NULL;
		  }
		  reflectiveColor = r_color;
		  transmissiveColor = t_color;
		  emittedColor = e_color;
		  roughness = roughness_;
		  indexOfRefraction = indexOfRefraction_;
		}
		~Material();

		// ACCESSORS
		const glm::vec3& getDiffuseColor() const { return diffuseColor; }
		const glm::vec3 getDiffuseColor(float s, float t) const;
		const glm::vec3& getReflectiveColor() const { return reflectiveColor; }
		const glm::vec3& getTransmissiveColor() const { return transmissiveColor; }
		const glm::vec3& getEmittedColor() const { return emittedColor; }
		float getRoughness() const { return roughness; }
		float getIOR() const { return indexOfRefraction; }
		bool hasTextureMap() const { return (textureFile != ""); }
		bool isReflective() const { return (reflectiveColor != glm::vec3(0,0,0)); }
		bool isTransmissive() const { return (transmissiveColor != glm::vec3(0,0,0)); }

		// SHADE
		// compute the contribution to local illumination at this point for
		// a particular light source
		glm::vec3 Shade
		(const Kinesis::Raytracing::Ray &ray, const Kinesis::Raytracing::Hit &hit, const glm::vec3 &dirToLight, 
		 const glm::vec3 &lightColor) const;
		
	  protected:
	  
		Material() { exit(0); }
		Material(const Material&) { exit(0); }
		const Material& operator=(const Material&) { exit(0); }
	  
		//void ComputeAverageTextureColor();
	  
		// REPRESENTATION
		glm::vec3 diffuseColor;
		glm::vec3 reflectiveColor;
		glm::vec3 transmissiveColor;
		glm::vec3 emittedColor;
		float roughness;
		double indexOfRefraction;
	  
		std::string textureFile;
		// Image *image;

	};
}

#endif // __MATERIAL_H__
