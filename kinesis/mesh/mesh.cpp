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
                              glm::vec3 color = glm::vec3(1.0f); // Default white

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


        std::cout << "  Loaded " << m_vertices.size() << " vertices and " << m_indices.size() << " indices." << std::endl;
        return true;
    }

    bool Mesh::parseMtl(const std::string& mtlFilePath, const std::string& /*basePath*/) {
        std::ifstream mtlfile(mtlFilePath);
        if (!mtlfile.is_open()) {
            std::cerr << "ERROR! Cannot open MTL file: " << mtlFilePath << std::endl;
            return false;
        }

        std::cout << "Parsing Material Library: " << mtlFilePath << std::endl;

        Material* currentMaterial = nullptr;
        std::string line;

        while (std::getline(mtlfile, line)) {
            std::stringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "newmtl") {
                std::string name;
                ss >> name;
                 // Create a new material (placeholder values)
                 currentMaterial = new Material(
                    name,
                    glm::vec3(0.8f, 0.8f, 0.8f), // Diffuse
                    glm::vec3(0.0f),             // Reflective
                    glm::vec3(0.0f),             // Transmissive
                    glm::vec3(0.0f),             // Emissive
                    0.5f,                        // Roughness
                    1.5f,                        // IOR
                    MaterialType::DIFFUSE        // Type
                );
                 m_materials.push_back(currentMaterial);
                 std::cout << "  Defined material: " << name << std::endl;
            } else if (token == "Kd" && currentMaterial) { // Diffuse color
                glm::vec3 color;
                ss >> color.x >> color.y >> color.z;
                // TODO: Update currentMaterial->diffuseColor (requires adding setter or modifying constructor/material management)
                std::cout << "    Kd: " << color.x << " " << color.y << " " << color.z << std::endl;
            } else if (token == "Ks" && currentMaterial) { // Specular color
                 glm::vec3 color;
                 ss >> color.x >> color.y >> color.z;
                 // TODO: Update currentMaterial->reflectiveColor
                 std::cout << "    Ks: " << color.x << " " << color.y << " " << color.z << std::endl;
            } else if (token == "Ke" && currentMaterial) { // Emissive color
                glm::vec3 color;
                ss >> color.x >> color.y >> color.z;
                 // TODO: Update currentMaterial->emissiveColor
                 std::cout << "    Ke: " << color.x << " " << color.y << " " << color.z << std::endl;
            } else if (token == "Ni" && currentMaterial) { // Index of Refraction
                 float ior;
                 ss >> ior;
                 // TODO: Update currentMaterial->indexOfRefraction
                 std::cout << "    Ni: " << ior << std::endl;
            } else if (token == "Ns" && currentMaterial) { // Specular Exponent (related to roughness, conversion needed)
                 float ns;
                 ss >> ns;
                 // Roughness = sqrt(2 / (Ns + 2)) is a common approximation
                 float roughness = sqrtf(2.0f / (ns + 2.0f));
                 // TODO: Update currentMaterial->roughness
                 std::cout << "    Ns: " << ns << " (Roughness approx: " << roughness << ")" << std::endl;
            } else if (token == "d" && currentMaterial) { // Dissolve (Opacity)
                 float d;
                 ss >> d;
                 // TODO: Handle opacity/transparency if needed
                 std::cout << "    d: " << d << std::endl;
            } else if (token == "Tr" && currentMaterial) { // Transparency (alternative to d)
                 float tr;
                 ss >> tr;
                 // TODO: Handle opacity/transparency if needed (Tr = 1-d)
                 std::cout << "    Tr: " << tr << std::endl;
             } else if (token == "illum" && currentMaterial) { // Illumination model
                  int model;
                  ss >> model;
                  // TODO: Set material type based on illumination model (e.g., illum 2 often means standard phong/blinn-phong)
                  std::cout << "    illum: " << model << std::endl;
             } else if (token == "map_Kd" && currentMaterial) { // Diffuse texture map
                 std::string texFile;
                 std::getline(ss, texFile);
                 texFile.erase(0, texFile.find_first_not_of(" \t"));
                  // TODO: Store texture filename in currentMaterial
                  std::cout << "    map_Kd: " << texFile << std::endl;
             }
             // Add handling for other MTL tokens (map_Ks, map_Ke, map_bump, etc.) as needed
        }

        mtlfile.close();
        std::cout << "Finished Parsing Material Library. Found " << m_materials.size() << " materials." << std::endl;
        return true; // Return true even if empty, indicates file was processed
    }

} // namespace Kinesis::Mesh