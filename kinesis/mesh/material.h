#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>
#include <glm/glm.hpp> // Make sure glm is included

// Forward declarations (if needed by other headers including this one,
// otherwise remove if Kinesis::Raytracing isn't directly used here)
// namespace Kinesis::Raytracing {
// 	class Hit;
// 	class Ray;
// }

namespace Kinesis::Mesh {

    // Material types likely used by the raytracer or deferred shader
    enum class MaterialType {
        DIFFUSE = 0,    // Standard non-metallic surface
        METAL = 1,      // Metallic surface
        DIELECTRIC = 2, // Glass, water, etc.
        LIGHT = 3       // Emissive surface (Optional: if needed)
        // Add other types as required by your raytracer/shaders
    };

	class Material {
	public:
		// Constructor initializes all relevant properties
		Material(const std::string &mat_name,         // Material name
            const glm::vec3 &d_color,      // Base diffuse/albedo color
            const glm::vec3 &r_color,      // Specular/Reflective color tint (Ks)
            const glm::vec3 &t_color,      // Transmissive color tint (Tf)
            const glm::vec3 &e_color,      // Emitted color (for lights)
            float roughness_,              // Surface roughness (0=smooth, 1=rough)
            float indexOfRefraction_,      // Index of Refraction (for dielectrics)
            MaterialType type_,            // The type of material
            const std::string &texture_path = "") : // Texture file path (optional)
       name(mat_name), // Store name
       diffuseColor(d_color),
       reflectiveColor(r_color),
       transmissiveColor(t_color),
       emittedColor(e_color),
       roughness(roughness_),
       indexOfRefraction(indexOfRefraction_),
       type(type_),
       textureFile(texture_path) // Store texture path
        {
             // Texture loading itself should happen in a dedicated texture manager
             // or be handled directly by the system loading the model/material.
             // This class now primarily stores the properties.
        }

		~Material() = default; // Default destructor is likely sufficient

		// --- ACCESSORS for properties needed by G-Buffer/Raytracer ---
        const std::string& getName() const { return name; } 
		const glm::vec3& getDiffuseColor() const { return diffuseColor; }
		const glm::vec3& getReflectiveColor() const { return reflectiveColor; }
		const glm::vec3& getTransmissiveColor() const { return transmissiveColor; }
		const glm::vec3& getEmittedColor() const { return emittedColor; }
		float getRoughness() const { return roughness; }
		float getIOR() const { return indexOfRefraction; }
        MaterialType getType() const { return type; }
		const std::string& getTextureFile() const { return textureFile; } // Access texture file path

		// Helper checks based on properties
		bool hasTextureMap() const { return !textureFile.empty(); }
        // Renamed to reflect common usage (PBR metallic workflow often uses base color + metallic flag)
		bool isMetallic() const { return type == MaterialType::METAL; }
		bool isTransmissive() const { return type == MaterialType::DIELECTRIC && transmissiveColor != glm::vec3(0.0f); }
        bool isEmissive() const { return emittedColor != glm::vec3(0.0f); } // Check if it emits light


        void setDiffuseColor(const glm::vec3 &color) { diffuseColor = color; }
        void setReflectiveColor(const glm::vec3 &color) { reflectiveColor = color; }
        void setRoughness(float r) { roughness = r; }
        void setEmittedColor(const glm::vec3 &color) { emittedColor = color; }
        void setIOR(float ior) { indexOfRefraction = ior; }
        void setType(MaterialType t) { type = t; }

	protected:
        // Prevent accidental copying if not intended, or implement proper copy semantics
		Material(const Material&) = delete;
		const Material& operator=(const Material&) = delete;
        // Allow moving
        Material(Material&&) = default;
        
        Material& operator=(Material&&) = default;


		// --- REPRESENTATION ---
		// Core material properties used by modern renderers
        std::string name;
		glm::vec3 diffuseColor;       // Base Color / Albedo
		glm::vec3 reflectiveColor;    // Specular Tint (often vec3(1) for pure metals, or tint for non-metals)
		glm::vec3 transmissiveColor;  // Color for transmitted light (refraction)
		glm::vec3 emittedColor;       // Light emission color and intensity (intensity baked into color value)
		float roughness;              // Surface roughness (for microfacet distribution)
		float indexOfRefraction;      // IOR (primarily for dielectrics)
        MaterialType type;            // Type identifier

		std::string textureFile;      // Path to diffuse texture (other maps might need separate strings or a struct)
                                      // Note: Actual VkImage/VkSampler handled externally

	}; // class Material
} // namespace Kinesis::Mesh

#endif // MATERIAL_H