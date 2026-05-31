#include "mesh.hpp"
#include <glad/glad.h>
#include "math_utils.hpp"
#include <iostream>
#include <fstream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <tuple>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <map>
#include <set>
#include <queue>
#include <chrono>
#include "../ui/app_console.hpp"

std::atomic<bool> Mesh::m_shouldTerminate { false };

Mesh::~Mesh() {
    FreeGPU();
}



void Mesh::UploadToGPU() {
    if (m_gpu.vao != 0) FreeGPU();

    glGenVertexArrays(1, &m_gpu.vao);
    glGenBuffers(1, &m_gpu.vbo);
    glGenBuffers(1, &m_gpu.ebo);

    glBindVertexArray(m_gpu.vao);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindVertexArray(0);
}

void Mesh::FreeGPU() {
    if (m_gpu.vao != 0) {
        glDeleteVertexArrays(1, &m_gpu.vao);
        glDeleteBuffers(1, &m_gpu.vbo);
        glDeleteBuffers(1, &m_gpu.ebo);
    }
    if (m_gpu.highlightVao != 0) {
        glDeleteVertexArrays(1, &m_gpu.highlightVao);
        glDeleteBuffers(1, &m_gpu.highlightEbo);
    }
    
    if (m_gpu.highlightEdgeVao != 0) {
        glDeleteVertexArrays(1, &m_gpu.highlightEdgeVao);
        glDeleteBuffers(1, &m_gpu.highlightEdgeEbo);
    }

    if (m_gpu.measuredVao != 0) {
        glDeleteVertexArrays(1, &m_gpu.measuredVao);
        glDeleteBuffers(1, &m_gpu.measuredEbo);
    }

    if (m_gpu.measuredEdgeVao != 0) {
        glDeleteVertexArrays(1, &m_gpu.measuredEdgeVao);
        glDeleteBuffers(1, &m_gpu.measuredEdgeEbo);
    }

    if (m_gpu.edgeVao != 0) {
        glDeleteVertexArrays(1, &m_gpu.edgeVao);
        glDeleteBuffers(1, &m_gpu.edgeEbo);
    }
    
    m_gpu.vao = 0;
    m_gpu.vbo = 0;
    m_gpu.ebo = 0;
    m_gpu.highlightVao = 0;
    m_gpu.highlightEbo = 0;
    m_gpu.highlightEdgeVao = 0;
    m_gpu.highlightEdgeEbo = 0;
    m_gpu.measuredVao = 0;
    m_gpu.measuredEbo = 0;
    m_gpu.measuredEdgeVao = 0;
    m_gpu.measuredEdgeEbo = 0;
    m_gpu.edgeVao = 0;
    m_gpu.edgeEbo = 0;
}

void Mesh::UpdateHighlightGPU() {
    if (highlightIndices.empty()) return;

    if (m_gpu.highlightVao == 0) {
        glGenVertexArrays(1, &m_gpu.highlightVao);
        glGenBuffers(1, &m_gpu.highlightEbo);
    }

    glBindVertexArray(m_gpu.highlightVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo); // Share the same VBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.highlightEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, highlightIndices.size() * sizeof(uint32_t), highlightIndices.data(), GL_DYNAMIC_DRAW);

    // Reuse vertex attributes from standard mesh
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindVertexArray(0);
    glBindVertexArray(0);
}

void Mesh::UpdateHighlightEdgeGPU() {
    if (highlightEdgeIndices.empty()) return;

    if (m_gpu.highlightEdgeVao == 0) {
        glGenVertexArrays(1, &m_gpu.highlightEdgeVao);
        glGenBuffers(1, &m_gpu.highlightEdgeEbo);
    }

    glBindVertexArray(m_gpu.highlightEdgeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo); // Share the same VBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.highlightEdgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, highlightEdgeIndices.size() * sizeof(uint32_t), highlightEdgeIndices.data(), GL_DYNAMIC_DRAW);

    // Reuse vertex attributes from standard mesh
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindVertexArray(0);
}

void Mesh::UpdateMeasuredGPU() {
    if (measuredIndices.empty()) return;
    if (m_gpu.measuredVao == 0) {
        glGenVertexArrays(1, &m_gpu.measuredVao);
        glGenBuffers(1, &m_gpu.measuredEbo);
        glBindVertexArray(m_gpu.measuredVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredEbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, measuredIndices.size() * sizeof(uint32_t), measuredIndices.data(), GL_DYNAMIC_DRAW);
}

void Mesh::UpdateMeasuredXRayGPU() {
    if (measuredXRayIndices.empty()) return;
    if (m_gpu.measuredXRayVao == 0) {
        glGenVertexArrays(1, &m_gpu.measuredXRayVao);
        glGenBuffers(1, &m_gpu.measuredXRayEbo);
        glBindVertexArray(m_gpu.measuredXRayVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredXRayEbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredXRayEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, measuredXRayIndices.size() * sizeof(uint32_t), measuredXRayIndices.data(), GL_DYNAMIC_DRAW);
}

void Mesh::UpdateMeasuredXRayEdgeGPU() {
    if (measuredXRayEdgeIndices.empty()) return;
    if (m_gpu.measuredXRayEdgeVao == 0) {
        glGenVertexArrays(1, &m_gpu.measuredXRayEdgeVao);
        glGenBuffers(1, &m_gpu.measuredXRayEdgeEbo);
        glBindVertexArray(m_gpu.measuredXRayEdgeVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredXRayEdgeEbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredXRayEdgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, measuredXRayEdgeIndices.size() * sizeof(uint32_t), measuredXRayEdgeIndices.data(), GL_DYNAMIC_DRAW);
}

void Mesh::UpdateMeasuredEdgeGPU() {
    if (measuredEdgeIndices.empty()) return;
    if (m_gpu.measuredEdgeVao == 0) {
        glGenVertexArrays(1, &m_gpu.measuredEdgeVao);
        glGenBuffers(1, &m_gpu.measuredEdgeEbo);
        glBindVertexArray(m_gpu.measuredEdgeVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gpu.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredEdgeEbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        // Normals are not strictly needed for lines, but keeping consistent with other edge rendering
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gpu.measuredEdgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, measuredEdgeIndices.size() * sizeof(uint32_t), measuredEdgeIndices.data(), GL_DYNAMIC_DRAW);
}

void Mesh::RecalculateBounds() {
    boundingBox.min = {1e9, 1e9, 1e9};
    boundingBox.max = {-1e9, -1e9, -1e9};

    for (const auto& v : vertices) {
        boundingBox.min = glm::min(boundingBox.min, Vec3(v.position));
        boundingBox.max = glm::max(boundingBox.max, Vec3(v.position));
    }

    // --- Metrological Density Analysis ---
    if (indices.size() >= 3) {
        double totalArea = 0.0;
        for (size_t i = 0; i < indices.size(); i += 3) {
            Vec3 v0(vertices[indices[i]].position);
            Vec3 v1(vertices[indices[i+1]].position);
            Vec3 v2(vertices[indices[i+2]].position);
            totalArea += glm::length(glm::cross(v1 - v0, v2 - v0)) * 0.5;
        }
        double avgArea = totalArea / (double)(indices.size() / 3);
        double bbSize = glm::length(boundingBox.Size());
        triangleDensity = (float)(avgArea / (bbSize + 1e-6));
    }
}






void Mesh::RecalculateNormals() {
    // Zero out all vertex normals
    for (auto& v : vertices) {
        v.normal = {0.0f, 0.0f, 0.0f};
    }
    
    // Accumulate face normal to all 3 connected vertices
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i+0];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];
        
        glm::vec3 v0(vertices[i0].position);
        glm::vec3 v1(vertices[i1].position);
        glm::vec3 v2(vertices[i2].position);
        
        // Ensure winding is correctly mapped cross(v1-v0, v2-v0)
        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        
        vertices[i0].normal += n;
        vertices[i1].normal += n;
        vertices[i2].normal += n;
    }
    
    // Normalize and convert back
    for (auto& v : vertices) {
        glm::vec3 n = glm::vec3(v.normal);
        float len = glm::length(n);
        if (len > 1e-6f) n /= len;
        else n = glm::vec3(0,1,0); // fallback
        v.normal = n;
    }
}





// ---------------------------------------------------------------------------
// Binary STL Export — vertices baked into world space via worldMatrix
// ---------------------------------------------------------------------------
bool Mesh::ExportSTL(const std::string& path, const Mat4f& worldMatrix) const {
    if (vertices.empty() || indices.size() < 3) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // 80-byte ASCII header
    char header[80] = {};
    std::string hdr = "Binary STL exported by Metrix3D - " + name;
    hdr.resize(79, ' ');
    std::memcpy(header, hdr.c_str(), 79);
    file.write(header, 80);

    // Triangle count
    uint32_t numTris = static_cast<uint32_t>(indices.size() / 3);
    file.write(reinterpret_cast<const char*>(&numTris), sizeof(numTris));

    uint16_t attrib = 0;

    for (uint32_t i = 0; i < numTris; ++i) {
        // Transform each vertex into world space
        auto xform = [&](uint32_t idx) -> glm::vec3 {
            glm::vec4 lp(vertices[idx].position, 1.0f);
            glm::vec4 wp = worldMatrix * lp;
            return glm::vec3(wp);
        };

        glm::vec3 v0 = xform(indices[i * 3 + 0]);
        glm::vec3 v1 = xform(indices[i * 3 + 1]);
        glm::vec3 v2 = xform(indices[i * 3 + 2]);

        // Face normal in world space
        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        file.write(reinterpret_cast<const char*>(&n),  sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&v0), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&v1), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&v2), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&attrib), sizeof(attrib));
    }

    return file.good();
}

void Mesh::BakeTransform(const Mat4f& worldMatrix) {
    if (vertices.empty()) return;

    // Normal transformation requires the inverse transpose of the sub-3x3 rotation/scale part
    Mat3 normalMatrix = glm::transpose(glm::inverse(Mat3(worldMatrix)));

    for (auto& v : vertices) {
        // Position baking
        glm::vec4 lp(v.position, 1.0f);
        glm::vec4 wp = worldMatrix * lp;
        v.position = glm::vec3(wp);

        // Normal baking
        glm::vec3 n = normalMatrix * glm::vec3(v.normal);
        float len = glm::length(n);
        if (len > 1e-6f) v.normal = n / len;
    }

    // Reset importOffset because this mesh is now in world coordinates
    importOffset = {0.0, 0.0, 0.0};

    // Recalculate bounds on baked mesh
    RecalculateBounds();
    
    // Flag for re-analysis as topography might have moved significantly in local space
    m_isAnalysisReady = false; 
    
    // Update GPU buffers
    UploadToGPU();
    
    AppConsole::Log("[Telemetry] Mesh '" + name + "' baked with world matrix. Transform reset in CPU memory.");
}

// ---------------------------------------------------------------------------
// MergeFrom — appends world-baked vertices from another mesh (for Merge tool)
// ---------------------------------------------------------------------------
void Mesh::MergeFrom(const Mesh& other, const Mat4f& otherWorld) {
    uint32_t base = static_cast<uint32_t>(vertices.size());

    for (const auto& v : other.vertices) {
        glm::vec4 lp(v.position, 1.0f);
        glm::vec4 wp = otherWorld * lp;
        Vertex nv;
        nv.position = glm::vec3(wp);
        // Transform normal: use the inverse-transpose of the rotation part
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(otherWorld)));
        nv.normal = glm::normalize(normalMat * glm::vec3(v.normal));
        vertices.push_back(nv);
    }

    for (uint32_t idx : other.indices) {
        indices.push_back(base + idx);
    }
}

std::shared_ptr<Mesh> Mesh::LoadFromFile(const std::string& path, bool centerMesh) {
    AppConsole::Log("[Import] Mesh::LoadFromFile: " + path);
    Assimp::Importer importer;
    // ... same loading code ...
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_Triangulate | 
        aiProcess_GenSmoothNormals | 
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return nullptr;
    }

    if (scene->mNumMeshes == 0) return nullptr;

    aiMesh* aimesh = scene->mMeshes[0];
    auto rawMesh = std::make_shared<Mesh>();

    // Always use the filename stem as the display name — the internal mesh name
    // written by CAD exporters (e.g. "CATIA V5", "SolidWorks") is not useful to users.
    {
        size_t lastSlash = path.find_last_of("/\\");
        std::string basename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        size_t dot = basename.rfind('.');
        rawMesh->name = (dot != std::string::npos) ? basename.substr(0, dot) : basename;
    }

    rawMesh->vertices.reserve(aimesh->mNumVertices);
    for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.position = {aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z};
        if (aimesh->HasNormals()) {
            vertex.normal = MathUtils::SafeNormalize({aimesh->mNormals[i].x, aimesh->mNormals[i].y, aimesh->mNormals[i].z});
        } else {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
        rawMesh->vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
        aiFace face = aimesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            rawMesh->indices.push_back(face.mIndices[j]);
        }
    }

    rawMesh->RecalculateBounds();

    // --- Materialise Magics behaviour: centre mesh at local origin ---
    if (centerMesh) {
        // Store the centroid as importOffset (needed to restore original coords on export)
        Vec3 centroid = rawMesh->boundingBox.Center();
        rawMesh->importOffset = centroid;

        // Shift every vertex so the bounding-box centre lands at (0, 0, 0)
        for (auto& v : rawMesh->vertices) {
            v.position.x -= (float)centroid.x;
            v.position.y -= (float)centroid.y;
            v.position.z -= (float)centroid.z;
        }

        // Recalculate bounds on centred mesh
        rawMesh->RecalculateBounds();
    } else {
        // Keep absolute coordinates
        rawMesh->importOffset = {0.0, 0.0, 0.0};
        AppConsole::Log("[Import] Absolute coordinates preserved (CAD/CAM mode).");
    }

    if (m_shouldTerminate) return rawMesh;
    rawMesh->BuildAdjacency();
    AppConsole::Log("[Telemetry] " + rawMesh->name + " - Adjacency Built.");
    
    if (m_shouldTerminate) return rawMesh;
    rawMesh->BuildBVH();
    AppConsole::Log("[Telemetry] " + rawMesh->name + " - BuildBVH OK.");
    
    // NOTE: AnalyzePrimitives is now called separately after UploadToGPU
    // rawMesh->AnalyzePrimitives(); 
    // AppConsole::Log("[Telemetry] " + rawMesh->name + " - AnalyzePrimitives OK.");
    
    if (m_shouldTerminate) return rawMesh;
    rawMesh->GenerateEdges(35.0f); // Default to 35 degree threshold on import
    AppConsole::Log("[Telemetry] " + rawMesh->name + " - Edge Generation OK.");

    // Initialize cluster IDs
    rawMesh->triangleClusterIds.assign(rawMesh->indices.size() / 3, -1);
    rawMesh->clusterToPrimitiveIdx.clear();

    // --- Contención RAM (Footprint Optimization) ---
    rawMesh->vertices.shrink_to_fit();
    rawMesh->indices.shrink_to_fit();
    rawMesh->bvhTree.shrink_to_fit();
    rawMesh->bvhIndices.shrink_to_fit();
    rawMesh->boundaryEdgeIndices.shrink_to_fit();
    
    // To shrink triangleNeighbors (vector of vectors), we must shrink internal vectors too
    for (auto& vec : rawMesh->triangleNeighbors) {
        vec.shrink_to_fit();
    }

    return rawMesh;
}



// ExtractFeatureLoop is now in TopologyEngine


void Mesh::BuildAdjacency() { TopologyEngine::BuildAdjacency(*this); }
void Mesh::BuildBVH() { TopologyEngine::BuildBVH(*this); }
void Mesh::AnalyzePrimitives(float timeLimitMs) { TopologyEngine::AnalyzePrimitives(*this, timeLimitMs); }
void Mesh::GenerateEdges(float angleThresholdDegrees) { TopologyEngine::GenerateEdges(*this, angleThresholdDegrees); }
void Mesh::SmoothMesh(int iterations, float lambda) { TopologyEngine::SmoothMesh(*this, iterations, lambda); }
int Mesh::FillHoles() { return TopologyEngine::FillHoles(*this); }
std::vector<uint32_t> Mesh::ExtractFeatureLoop(uint32_t v0, uint32_t v1) const { return TopologyEngine::ExtractFeatureLoop(*this, v0, v1); }
