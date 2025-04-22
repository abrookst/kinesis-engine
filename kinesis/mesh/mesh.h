#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include <memory> // For std::unique_ptr if managing materials internally
#include <glm/glm.hpp>

// Forward declarations (adjust namespaces if needed)
namespace Kinesis::Mesh {
    class Vertex;
    class Material;
}

// ==========================================================
namespace Kinesis::Mesh {

    class Mesh {
    public:
        // --- Constructor & Destructor ---
        Mesh() = default; // Default constructor
        ~Mesh(); // Destructor to clean up materials

        // --- Loading ---
        // Loads geometry and materials from an OBJ file and its associated MTL file (implied).
        // Returns true on success, false on failure.
        bool Load(const std::string &objFilePath);

        // --- Accessors ---
        const std::vector<Vertex>& getVertices() const { return m_vertices; }
        const std::vector<uint32_t>& getIndices() const { return m_indices; }
        const std::vector<Material*>& getMaterials() const { return m_materials; } // Access loaded materials

        size_t numVertices() const { return m_vertices.size(); }
        size_t numIndices() const { return m_indices.size(); }
        bool hasIndices() const { return !m_indices.empty(); }

        // Optional: Add access to background color if still needed
        // const glm::vec3& getBackgroundColor() const { return m_backgroundColor; }

        // --- Deleted copy/move semantics (or define them properly if needed) ---
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) = default; // Allow moving
        Mesh& operator=(Mesh&&) = default; // Allow moving

    private:
        // --- Representation ---
        std::vector<Vertex> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<Material*> m_materials; // Owns the materials loaded from the file

        bool parseMtl(const std::string& mtlFilePath, const std::string& basePath);

    }; // class Mesh

} 
#endif 