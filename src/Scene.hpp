#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <raylib.h>
#include "BinReader.hpp"
#include "types.hpp"

struct MeshVertex {
    Vec3f pos;
    Vec3f normal;
    Col4u color;
    Vec2f uv;
};

struct MeshPart {
    unsigned int vertexBufferID;
    unsigned int indexBufferID;
    unsigned int indexOffset;
    unsigned int indexCount;
    unsigned int vertexOffset;
    unsigned int vertexCount;
};

class Scene {
  public:
    Scene();
    ~Scene();
    void load(const std::string& filename);

    void render();

  private:
    void loadVertices(BinReader& reader, MeshPart& part);
    void loadIndices(BinReader& reader, MeshPart& part);
    void genMesh(const MeshPart& part);
    void readPart(BinReader& reader);
    void cleanup();

    std::vector<Model> m_models;
    std::unordered_map<unsigned int, std::vector<MeshVertex>> m_vertexBuffers;
    std::unordered_map<unsigned int, std::vector<unsigned short>> m_indexBuffers;
    unsigned int m_refCounter;
};