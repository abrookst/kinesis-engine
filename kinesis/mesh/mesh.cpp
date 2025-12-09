#include "mesh.h"
#include "vertex.h"   // Make sure vertex.h is included
#include "material.h" // Make sure material.h is included

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem> // For path manipulation
#include <vector>
#include <map> // Useful for index lookup during parsing


namespace Kinesis::Mesh {

    Mesh::~Mesh() {
        // Clean up owned materials
        for (Material* mat : m_materials) {
            delete mat;
        }
        m_materials.clear();
    }

    bool Mesh::Load(const std::string &objFilePath) {
        m_vertices.clear();
        m_indices.clear();
        for (Material* mat : m_materials) { delete mat; } // Clear old materials
        m_materials.clear();

        std::ifstream objfile(objFilePath);
        if (!objfile.is_open()) {
            std::cerr << "ERROR! Cannot open OBJ file: " << objFilePath << std::endl;
            return false;
        }

        std::cout << "Loading Mesh: " << objFilePath << std::endl;

        std::filesystem::path objPath(objFilePath);
        std::string basePath = objPath.parent_path().string();

        std::vector<glm::vec3> temp_positions;
        std::vector<glm::vec2> temp_texCoords;
        std::vector<glm::vec3> temp_normals;

        std::string line;
        Material* currentMaterial = nullptr; // Track the active material

        // Simple map to reuse vertices based on v/vt/vn combo
        std::map<std::string, uint32_t> vertex_map;

        while (std::getline(objfile, line)) {
            std::stringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "v") {
                glm::vec3 pos;
                ss >> pos.x >> pos.y >> pos.z;
                temp_positions.push_back(pos);
            } else if (token == "vt") {
                glm::vec2 uv;
                ss >> uv.x >> uv.y;
                 // Optional: Flip V coordinate if needed (common difference between formats)
                 // uv.y = 1.0f - uv.y;
                temp_texCoords.push_back(uv);
            } else if (token == "vn") {
                glm::vec3 norm;
                ss >> norm.x >> norm.y >> norm.z;
                temp_normals.push_back(norm);
            } else if (token == "f") {
                // Process face (assuming triangles v/vt/vn)
                // TODO: Handle polygons with more than 3 vertices (triangulate)
                std::string v1_token, v2_token, v3_token, extra_token;
                ss >> v1_token >> v2_token >> v3_token >> extra_token; // Read potential 4th vertex

                 if (!extra_token.empty()) {
                     // Basic triangulation: Assume convex quad and split 0-1-2, 0-2-3
                     // More robust triangulation might be needed for complex polygons
                     std::cerr << "Warning: Face with more than 3 vertices encountered near line: '" << line << "'. Performing basic triangulation." << std::endl;
                     std::vector<std::string> face_tokens = {v1_token, v2_token, v3_token, extra_token};
                      // First triangle (0, 1, 2)
                      for (int i = 0; i < 3; ++i) {
                           // Process face_tokens[i] (same logic as below)
                           if (vertex_map.count(face_tokens[i])) { m_indices.push_back(vertex_map[face_tokens[i]]); }
                            else { /* Create new vertex and add index */
                                std::stringstream face_ss(face_tokens[i]); std::string segment; std::vector<int> indices;
                                while (std::getline(face_ss, segment, '/')) { indices.push_back(segment.empty() ? 0 : std::stoi(segment)); }
                                int v_idx = (indices.size() > 0 && indices[0] > 0) ? indices[0] - 1 : -1;
                                int vt_idx = (indices.size() > 1 && indices[1] > 0) ? indices[1] - 1 : -1;
                                int vn_idx = (indices.size() > 2 && indices[2] > 0) ? indices[2] - 1 : -1;
                                glm::vec3 pos = (v_idx >= 0 && v_idx < temp_positions.size()) ? temp_positions[v_idx] : glm::vec3(0.0f);
                                glm::vec2 uv = (vt_idx >= 0 && vt_idx < temp_texCoords.size()) ? temp_texCoords[vt_idx] : glm::vec2(0.0f);
                                glm::vec3 norm = (vn_idx >= 0 && vn_idx < temp_normals.size()) ? temp_normals[vn_idx] : glm::vec3(0.0f, 1.0f, 0.0f);
                                glm::vec3 color = glm::vec3(1.0f);
                                uint32_t new_index = static_cast<uint32_t>(m_vertices.size());
                                m_vertices.emplace_back(new_index, pos, color, norm, uv);
                                m_indices.push_back(new_index); vertex_map[face_tokens[i]] = new_index;
                            }
                       }
                        // Second triangle (0, 2, 3)
                       std::vector<int> triangle2_indices = {0, 2, 3};
                       for (int i_idx : triangle2_indices) {
                           // Process face_tokens[i_idx] (same logic as above)
                           if (vertex_map.count(face_tokens[i_idx])) { m_indices.push_back(vertex_map[face_tokens[i_idx]]); }
                           else { /* Create new vertex and add index */
                                std::stringstream face_ss(face_tokens[i_idx]); std::string segment; std::vector<int> indices;
                                while (std::getline(face_ss, segment, '/')) { indices.push_back(segment.empty() ? 0 : std::stoi(segment)); }
                                int v_idx = (indices.size() > 0 && indices[0] > 0) ? indices[0] - 1 : -1;
                                int vt_idx = (indices.size() > 1 && indices[1] > 0) ? indices[1] - 1 : -1;
                                int vn_idx = (indices.size() > 2 && indices[2] > 0) ? indices[2] - 1 : -1;
                                glm::vec3 pos = (v_idx >= 0 && v_idx < temp_positions.size()) ? temp_positions[v_idx] : glm::vec3(0.0f);
                                glm::vec2 uv = (vt_idx >= 0 && vt_idx < temp_texCoords.size()) ? temp_texCoords[vt_idx] : glm::vec2(0.0f);
                                glm::vec3 norm = (vn_idx >= 0 && vn_idx < temp_normals.size()) ? temp_normals[vn_idx] : glm::vec3(0.0f, 1.0f, 0.0f);
                                glm::vec3 color = glm::vec3(1.0f);
                                uint32_t new_index = static_cast<uint32_t>(m_vertices.size());
                                m_vertices.emplace_back(new_index, pos, color, norm, uv);
                                m_indices.push_back(new_index); vertex_map[face_tokens[i_idx]] = new_index;
                            }
                       }

                 } else {
                      // Process triangle face_tokens = {v1_token, v2_token, v3_token}
                      std::vector<std::string> face_tokens = {v1_token, v2_token, v3_token};
                      for (const std::string& face_token : face_tokens) {
                          // --- Vertex Deduplication ---
                          if (vertex_map.count(face_token)) {
                              m_indices.push_back(vertex_map[face_token]);
                          } else {
                              // Create a new vertex
                              std::stringstream face_ss(face_token);
                              std::string segment;
                              std::vector<int> indices; // Stores v, vt, vn indices (1-based)

                              while (std::getline(face_ss, segment, '/')) {
                                  if (!segment.empty()) {
                                      indices.push_back(std::stoi(segment));
                                  } else {
                                       indices.push_back(0); // Handle cases like "v//vn"
                                  }
                              }

                              // Get data from temp arrays using 1-based indices from OBJ
                              int v_idx = (indices.size() > 0 && indices[0] > 0) ? indices[0] - 1 : -1;
                              int vt_idx = (indices.size() > 1 && indices[1] > 0) ? indices[1] - 1 : -1;
                              int vn_idx = (indices.size() > 2 && indices[2] > 0) ? indices[2] - 1 : -1;

                              glm::vec3 pos = (v_idx >= 0 && v_idx < temp_positions.size()) ? temp_positions[v_idx] : glm::vec3(0.0f);
                              glm::vec2 uv = (vt_idx >= 0 && vt_idx < temp_texCoords.size()) ? temp_texCoords[vt_idx] : glm::vec2(0.0f);
                              glm::vec3 norm = (vn_idx >= 0 && vn_idx < temp_normals.size()) ? temp_normals[vn_idx] : glm::vec3(0.0f, 1.0f, 0.0f); // Default normal if missing

                              // Assign a default color or color based on material later
                              glm::vec3 color = glm::vec3(1.0f, 0.f, 0.f); // debugi red

                              // Assign an index for the new vertex
                              uint32_t new_index = static_cast<uint32_t>(m_vertices.size());
                              m_vertices.emplace_back(new_index, pos, color, norm, uv); // Use the Vertex constructor
                              m_indices.push_back(new_index);
                              vertex_map[face_token] = new_index;
                          }
                      } // end for face_token
                 } // end else (triangle face)

            } else if (token == "mtllib") {
                 std::string mtlFileName;
                 // Handle potential spaces in filename if the rest of the line is the filename
                 std::getline(ss, mtlFileName);
                 // Trim leading whitespace if any from getline
                  mtlFileName.erase(0, mtlFileName.find_first_not_of(" \t"));

                 if (!mtlFileName.empty()) {
                     std::string mtlFilePath = basePath.empty() ? mtlFileName : basePath + "/" + mtlFileName; // Combine paths
                     std::cout << "  Found Material Library: " << mtlFilePath << std::endl;
                     // TODO: Implement MTL parsing function call here
                     parseMtl(mtlFilePath, basePath);
                     // For now, create a placeholder default material
                 } else {
                      std::cerr << "Warning: mtllib token found but no filename specified." << std::endl;
                 }


            } else if (token == "usemtl") {
                std::string materialName;
                 // Handle potential spaces in material name
                 std::getline(ss, materialName);
                 materialName.erase(0, materialName.find_first_not_of(" \t"));

                if (!materialName.empty()){
                    // TODO: Find the loaded material by name and set currentMaterial
                    // For now, just use the first material if available
                    if (!m_materials.empty()) {
                        currentMaterial = m_materials[0]; // Assign the current material
                    } else {
                         // Create default material if none loaded yet
                          m_materials.push_back(new Material("", {0.8f, 0.8f, 0.8f}, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f), 0.5f, 1.5f, MaterialType::DIFFUSE));
                          currentMaterial = m_materials[0];
                          std::cout << "  No materials loaded, creating default for: " << materialName << std::endl;
                    }
                    std::cout << "  Using material (placeholder assignment): " << materialName << std::endl;
                 } else {
                     std::cerr << "Warning: usemtl token found but no material name specified." << std::endl;
                 }
            }
             // Handle other tokens like 's', 'g', 'o', comments '#' etc. if needed
        }

        objfile.close();

        if (m_vertices.empty()) {
            std::cerr << "Warning: No vertices loaded from " << objFilePath << std::endl;
            // Return true because the file was opened, but signal empty mesh?
            // Or return false if an empty mesh is an error.
            return true; // Let's allow empty meshes for now
        }
         if (m_indices.empty()) {
             std::cerr << "Warning: No indices loaded from " << objFilePath << "." << std::endl;
             // Don't automatically generate indices if faces weren't defined.
             // If vertices exist but indices don't, it might be a point cloud or line list.
         }

        // Compute normals from geometry if they weren't provided in the OBJ file
        std::cout << "  Computing normals from geometry..." << std::endl;
        computeNormals();

        std::cout << "  Loaded " << m_vertices.size() << " vertices and " << m_indices.size() << " indices." << std::endl;
        return true;
    }

    bool Mesh::parseMtl(const std::string& mtlFilePath, const std::string& basePath) { // Keep basePath if needed for textures
        std::ifstream mtlfile(mtlFilePath);
        if (!mtlfile.is_open()) {
            std::cerr << "ERROR! Cannot open MTL file: " << mtlFilePath << std::endl;
            return false;
        }

        std::cout << "Parsing Material Library: " << mtlFilePath << std::endl;

        Material* currentMaterial = nullptr;
        std::string line;

        // Default values for a new material
        std::string name = "default";
        glm::vec3 diffuseColor(0.8f);
        glm::vec3 specularColor(0.0f); // Ks
        glm::vec3 transmissiveColor(0.0f); // Tf (often used for transmission filter)
        glm::vec3 emissiveColor(0.0f); // Ke
        float roughness = 0.8f; // Derived from Ns later
        float ior = 1.5f; // Ni - default glass IOR
        float opacity = 1.0f; // d
        int illumModel = 1; // Default illum
        std::string textureFile = "";
        MaterialType matType = MaterialType::DIFFUSE; // Default type

        while (std::getline(mtlfile, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty() || line[0] == '#') continue;

            std::stringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "newmtl") {
                // If we were defining a previous material, finalize and add it
                if (currentMaterial) {
                    // Update properties based on parsed values BEFORE creating the new one
                     // Note: Material constructor needs updating or setters are needed
                     // This example assumes we create the material *after* parsing its block
                }

                 // Reset defaults for the new material
                 ss >> std::ws >> name; // Read name
                 diffuseColor = glm::vec3(0.8f);
                 specularColor = glm::vec3(0.0f);
                 transmissiveColor = glm::vec3(0.0f);
                 emissiveColor = glm::vec3(0.0f);
                 roughness = 0.8f;
                 ior = 1.5f;
                 opacity = 1.0f;
                 illumModel = 1;
                 textureFile = "";
                 matType = MaterialType::DIFFUSE;

                 // Create a placeholder - we'll configure it as we parse
                 // Or, better: store parsed values and create the Material object at the END
                 // of the file or before the next "newmtl"
                 std::cout << "  Defining material: " << name << std::endl;


            } else if (token == "Kd") { // Diffuse color
                 if (!(ss >> diffuseColor.x >> diffuseColor.y >> diffuseColor.z)) {
                      std::cerr << "    Warning: Malformed Kd in material " << name << std::endl;
                 }
            } else if (token == "Ks") { // Specular color (can indicate metalness or just specularity)
                 if (!(ss >> specularColor.x >> specularColor.y >> specularColor.z)) {
                     std::cerr << "    Warning: Malformed Ks in material " << name << std::endl;
                 }
            } else if (token == "Ke") { // Emissive color
                 if (!(ss >> emissiveColor.x >> emissiveColor.y >> emissiveColor.z)) {
                      std::cerr << "    Warning: Malformed Ke in material " << name << std::endl;
                 }
            } else if (token == "Tf") { // Transmission Filter (often indicates dielectric color)
                 if (!(ss >> transmissiveColor.x >> transmissiveColor.y >> transmissiveColor.z)) {
                     std::cerr << "    Warning: Malformed Tf in material " << name << std::endl;
                 }
            } else if (token == "Ni") { // Index of Refraction
                 if (!(ss >> ior)) {
                      std::cerr << "    Warning: Malformed Ni in material " << name << std::endl;
                      ior = 1.5f; // Reset to default on error
                 }
                 // Clamp IOR to reasonable values if needed
                 ior = glm::max(1.0f, ior);
            } else if (token == "Ns") { // Specular Exponent
                 float ns;
                 if (!(ss >> ns)) {
                      std::cerr << "    Warning: Malformed Ns in material " << name << std::endl;
                      ns = 10.0f; // Default exponent
                 }
                 // Convert Ns to roughness (approximation)
                 roughness = sqrtf(2.0f / (glm::max(2.0f, ns) + 2.0f)); // Clamp Ns >= 2
                 // roughness = 1.0f - (ns / 1000.0f); // Another simpler approx if Ns range is known
                 roughness = glm::clamp(roughness, 0.01f, 1.0f); // Clamp roughness
                 // std::cout << "    Ns: " << ns << " -> Roughness approx: " << roughness << std::endl;
            } else if (token == "d") { // Dissolve (Opacity)
                 if (!(ss >> opacity)) {
                     std::cerr << "    Warning: Malformed d in material " << name << std::endl;
                     opacity = 1.0f;
                 }
                 opacity = glm::clamp(opacity, 0.0f, 1.0f);
            } else if (token == "Tr") { // Transparency (alternative to d)
                 float tr;
                 if (!(ss >> tr)) {
                      std::cerr << "    Warning: Malformed Tr in material " << name << std::endl;
                      tr = 0.0f;
                 }
                 opacity = 1.0f - glm::clamp(tr, 0.0f, 1.0f); // Tr = 1 - d
             } else if (token == "illum") { // Illumination model
                  if (!(ss >> illumModel)) {
                     std::cerr << "    Warning: Malformed illum in material " << name << std::endl;
                     illumModel = 1;
                  }
             } else if (token == "map_Kd") { // Diffuse texture map
                 std::getline(ss >> std::ws, textureFile);
                 // Prepend basePath if textureFile is not absolute
                 if (!textureFile.empty() && std::filesystem::path(textureFile).is_relative()) {
                    textureFile = (std::filesystem::path(basePath) / textureFile).string();
                 }
                 std::cout << "    map_Kd: " << textureFile << std::endl;
             }
             // Add handling for other MTL tokens (map_Ks, map_Ke, map_bump, etc.) as needed

             // Check if end of file or next material is starting
             std::streampos currentPos = mtlfile.tellg(); // Remember position
             std::string nextLine;
             bool nextIsNewMtl = false;
             if (std::getline(mtlfile, nextLine)) {
                 std::stringstream next_ss(nextLine);
                 std::string next_token;
                 next_ss >> next_token;
                 if (next_token == "newmtl") {
                     nextIsNewMtl = true;
                 }
                 // Crucially, put the line back or seek back
                 mtlfile.seekg(currentPos);
             } else {
                nextIsNewMtl = true; // Treat EOF as end of current material block
             }

            // If the next line starts a new material or we are at EOF,
            // finalize the current material definition
            if (nextIsNewMtl && !name.empty()) { // Make sure we have a name
                 // Determine MaterialType based on parsed properties
                 if (glm::length(emissiveColor) > 0.1f) { // Check if emissive
                    matType = MaterialType::LIGHT;
                 } else if (opacity < 0.95f || illumModel == 5 || illumModel == 7 || glm::length(transmissiveColor) > 0.1f) {
                    // Heuristic for dielectric: transparent, or specific illum models, or has Tf color
                    matType = MaterialType::DIELECTRIC;
                    diffuseColor = glm::vec3(1.0f); // Dielectrics often use baseColor=1 and rely on Tf/transmission
                    if (glm::length(transmissiveColor) > 0.1f) {
                        // If Tf is set, use it as the base color (filter color)
                        diffuseColor = transmissiveColor;
                    }
                 } else if (illumModel == 3 || (glm::length(specularColor) > 0.1f && diffuseColor.r < 0.1f && diffuseColor.g < 0.1f && diffuseColor.b < 0.1f )) {
                     // Heuristic for metal: illum 3 (raytrace reflection) or high specularity with low diffuse
                    matType = MaterialType::METAL;
                    diffuseColor = specularColor; // Metals use specular color as base color
                 } else {
                    matType = MaterialType::DIFFUSE; // Default
                 }


                // Create and add the material
                 // This requires modifying the Material constructor or adding setters
                 // Example assuming constructor takes all needed values:
                currentMaterial = new Material(
                    name,           // Use parsed name (Need to add name to Material class)
                    diffuseColor,
                    specularColor,      // Pass specular color (might be used for metal tint or dielectric reflection)
                    transmissiveColor,  // Pass transmissive color
                    emissiveColor,
                    roughness,
                    ior,
                    matType,
                    textureFile         // Pass texture file (Need to add to Material class/constructor)
                );
                m_materials.push_back(currentMaterial);
                std::cout << "    -> Finalized as Type: " << static_cast<int>(matType) << ", IOR: " << ior << ", Roughness: " << roughness << std::endl;

                // Reset name to prevent adding the same material multiple times if file ends unexpectedly
                name = "";
                currentMaterial = nullptr; // Ready for the next 'newmtl'
            }
        } // End while loop

        mtlfile.close();
        std::cout << "Finished Parsing Material Library. Added " << m_materials.size() << " materials." << std::endl;
        return true; // Return true even if empty, indicates file was processed
    }

    // Compute normals from geometry when OBJ file doesn't include them
    void Mesh::computeNormals() {
        // First, initialize all normals to zero
        for (auto& vertex : m_vertices) {
            vertex.normal = glm::vec3(0.0f);
        }

        // Compute face normals and accumulate to vertex normals
        for (size_t i = 0; i < m_indices.size(); i += 3) {
            uint32_t idx0 = m_indices[i];
            uint32_t idx1 = m_indices[i + 1];
            uint32_t idx2 = m_indices[i + 2];

            glm::vec3 v0 = m_vertices[idx0].position;
            glm::vec3 v1 = m_vertices[idx1].position;
            glm::vec3 v2 = m_vertices[idx2].position;

            // Compute face normal using cross product
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::cross(edge1, edge2);

            // Accumulate face normal to each vertex of the triangle
            m_vertices[idx0].normal += faceNormal;
            m_vertices[idx1].normal += faceNormal;
            m_vertices[idx2].normal += faceNormal;
        }

        // Normalize all vertex normals
        for (auto& vertex : m_vertices) {
            if (glm::length(vertex.normal) > 0.0f) {
                vertex.normal = glm::normalize(vertex.normal);
            } else {
                // Fallback for degenerate cases
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        std::cout << "Computed normals for " << m_vertices.size() << " vertices." << std::endl;
    }

} // namespace Kinesis::Mesh