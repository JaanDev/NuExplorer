#include "Scene.hpp"

#include "logger.hpp"
#include "BinReader.hpp"
#include "utils.hpp"
#include "umHalf.h"

struct MeshAttrib {
    MeshValType valType; // position, normal, etc
    MeshVarType varType; // vec4half, vec2mini, etc
};

Scene::Scene() : m_refCounter(7) {}

Scene::~Scene() {
    cleanup();
}

void Scene::render() {
    for (const auto& model : m_models) {
        DrawModel(model, {0, 0, 0}, 1.f, WHITE);
    }
}

void Scene::load(const std::string& filename) {
    cleanup();
    m_models.clear();
    m_refCounter = 7;
    m_vertexBuffers.clear();
    m_indexBuffers.clear();

    logD("Loading a scene from {}", filename);

    auto reader = BinReader(filename, Endianness::Big);

    // load TXGH
    auto txghOffset = reader.find(std::string_view("HGXT", 4));
    logD("TXGH: Found at 0x{:08X}", txghOffset);
    reader.seek(txghOffset);
    reader.skip(4 + 4 + 4);
    uint32_t texCount;
    reader >> texCount;
    m_refCounter += texCount; // i have 0 idea about how the heck this works but it does
    reader.skip(texCount * 4 + 4);
    reader >> texCount;
    for (auto i = 0u; i < texCount; i++) {
        reader.skip(16);
        uint32_t nameLen;
        reader >> nameLen;
        reader.skip(nameLen + 2 + 1 + 1);
    }
    reader.skip(4);
    uint32_t unk;
    reader >> unk;
    m_refCounter += unk; // ¯\_(ツ)_/¯

    // loading MESH
    auto meshOffset = reader.find(std::string_view("HSEM", 4));
    logD("MESH: Found at 0x{:08X}", meshOffset);
    reader.seek(meshOffset);
    reader.skip(4); // MESH 4cc
    uint32_t ver;
    reader >> ver;
    logD("MESH: Version: 0x{:X}", ver);
    if (ver < 0x30) {
        logE("MESH: Supported version is 0x30 only!");
        exit(1);
    }
    reader.skip(4); // ROTV
    uint32_t len;   // parts count
    reader >> len;
    logD("MESH: {} parts", len);

    for (auto i = 0u; i < len; i++) {
        logD("======= loading part {} =======", i);
        readPart(reader);
    }
}

void Scene::loadVertices(BinReader& reader, MeshPart& part) {
    // VERTICES
    uint32_t size;
    reader >> size;
    assert(size == 1 || size == 2);

    // VERTEX_SOMETHING
    std::vector<MeshVertex> vertices;
    part.vertexBufferID = m_refCounter;

    for (auto i = 0; i < size; i++) {
        uint32_t unk;
        reader >> unk;
        if (unk >> 3 * 8 == 0xC0) {
            auto id = unk ^ 0xC0'00'00'00;
            logD("reusing vertex buffer 0x{:X}", id);
            if (i == 0)
                part.vertexBufferID = id;
            reader.skip(4 + 4);
            continue;
        }
        if (!unk)
            continue;

        reader.skip(4); // flags
        uint32_t count;
        reader >> count;
        logD("new vertex buffer 0x{:X}", m_refCounter);
        logD("MESH: Part vertex count: {}", count);

        uint32_t nbAttribs;
        reader >> nbAttribs;

        std::vector<MeshAttrib> attribs;

        for (auto i = 0u; i < nbAttribs; i++) {
            MeshAttrib attrib;
            reader >> attrib.valType >> attrib.varType;
            logD("attrib: {} {}", (int)attrib.valType, (int)attrib.varType);
            reader.skip(1); // offset of the attrib
            attribs.push_back(attrib);
        }

        for (auto i = 0u; i < count; i++) {
            MeshVertex vertex;
            vertex.pos = {0, 0, 0};
            vertex.normal = {0, 0, 0};
            vertex.uv = {0, 0};
            vertex.color = {255, 255, 255, 255};

            for (const auto& attrib : attribs) {
                switch (attrib.valType) {
                case MeshValType::Position: {
                    switch (attrib.varType) {
                    case MeshVarType::Vec4half: { // what
                        half x, y, z;
                        reader >> x >> y >> z;
                        reader.skip(2);
                        vertex.pos.x = (float)x;
                        vertex.pos.y = (float)y;
                        vertex.pos.z = (float)z;
                    } break;
                    case MeshVarType::Vec3f:
                        // float x, y, z;
                        reader >> vertex.pos.x >> vertex.pos.y >> vertex.pos.z;
                        // reader >> x >> y >> z;
                        // vertex.pos = {x, y, z};
                        break;
                    default:
                        logE("Unhandled `position` varType: {}", (int)attrib.varType);
                        reader.skip(utils::getVarSize(attrib.varType));
                        break;
                    }
                } break;
                case MeshValType::Normal: {
                    switch (attrib.varType) {
                    // case MeshVarType::Vec3f:
                    //     reader >> vertex.normal.x >> vertex.normal.y >> vertex.normal.z;
                    //     break;
                    // case MeshVarType::Vec4f:
                    //     reader >> vertex.normal.x >> vertex.normal.y >> vertex.normal.z;
                    //     reader.skip(4);
                    //     break;
                    case MeshVarType::Vec4mini:
                        unsigned char x, y, z;
                        reader >> x >> y >> z;
                        reader.skip(1); // w
                        vertex.normal.x = utils::getMiniFloat(x);
                        vertex.normal.y = utils::getMiniFloat(y);
                        vertex.normal.z = utils::getMiniFloat(z);
                        break;
                    default:
                        logE("Unhandled `normal` varType: {}", (int)attrib.varType);
                        reader.skip(utils::getVarSize(attrib.varType));
                        break;
                    }
                } break;
                case MeshValType::ColorSet0: {
                    switch (attrib.varType) {
                    case MeshVarType::Col4char:
                        reader >> vertex.color.r >> vertex.color.g >> vertex.color.b >> vertex.color.a;
                        break;
                    default:
                        logE("Unhandled `color` varType: {}", (int)attrib.varType);
                        reader.skip(utils::getVarSize(attrib.varType));
                        break;
                    }
                } break;
                case MeshValType::UVSet1: {
                    switch (attrib.varType) {
                    case MeshVarType::Vec4half: { // what
                        half x, y;
                        reader >> x >> y;
                        reader.skip(4);
                        vertex.uv.x = (float)x;
                        vertex.uv.y = (float)y;
                    } break;
                    case MeshVarType::Vec2half: {
                        half x, y;
                        reader >> x >> y;
                        vertex.uv.x = (float)x;
                        vertex.uv.y = (float)y;
                    } break;
                    default:
                        logE("Unhandled `uv` varType: {}", (int)attrib.varType);
                        reader.skip(utils::getVarSize(attrib.varType));
                        break;
                    }
                } break;
                case MeshValType::Tangent:
                case MeshValType::ColorSet1:
                case MeshValType::Unknown:
                case MeshValType::UVSet2:
                case MeshValType::Unknown2:
                case MeshValType::BlendIndices:
                case MeshValType::BlendWeight:
                case MeshValType::Unknown3:
                case MeshValType::LightDirSet:
                case MeshValType::LightColSet:
                    // unused
                    reader.skip(utils::getVarSize(attrib.varType));
                    break;
                default:
                    logE("Unhandled valType {}", (int)attrib.valType);
                    break;
                }
            }

            vertices.push_back(vertex);
        }

        reader.skip(4); // byteOffset

        if (i == 0)
            m_vertexBuffers[m_refCounter] = vertices;
        m_refCounter++;
    }
}

void Scene::loadIndices(BinReader& reader, MeshPart& part) {
    uint32_t something;
    reader >> something;

    if (something >> 3 * 8 == 0xC0) { // reusing an index buffer
        auto id = something ^ 0xC0'00'00'00;
        logD("reusing index buffer 0x{:x}", id);
        part.indexBufferID = id;
        reader.skip(4);
    } else {
        reader.skip(4); // flags
        uint32_t count, size;
        reader >> count >> size;
        logD("new index buffer {:X}", m_refCounter);
        logD("MESH: Part index count: {}", count);
        assert(size == 2);

        std::vector<unsigned short> indices;

        indices.resize(count);

        for (auto i = 0u; i < count; i++) {
            reader >> indices[i];
        }

        part.indexBufferID = m_refCounter;
        m_indexBuffers[m_refCounter] = indices;
        m_refCounter++;
    }

    reader >> part.indexOffset >> part.indexCount >> part.vertexOffset;
    reader.skip(2);
    reader >> part.vertexCount;
    logD("indexOffset {}, indexCount {}, verticesOffset {}, verticesCount {}", part.indexOffset, part.indexCount,
         part.vertexOffset, part.vertexCount);
}

void Scene::genMesh(const MeshPart& part) {
    Mesh mesh = {0};

    mesh.vertexCount = part.vertexCount;
    mesh.triangleCount = part.indexCount / 3;
    logD("vertex count {} tri count {}", mesh.vertexCount, mesh.triangleCount);

    mesh.vertices = (float*)MemAlloc(sizeof(float) * 3 * mesh.vertexCount);
    mesh.normals = (float*)MemAlloc(sizeof(float) * 3 * mesh.vertexCount);
    mesh.texcoords = (float*)MemAlloc(sizeof(float) * 2 * mesh.vertexCount);
    mesh.colors = (uint8_t*)MemAlloc(sizeof(uint8_t) * 4 * mesh.vertexCount);
    mesh.indices = (unsigned short*)MemAlloc(sizeof(unsigned short) * part.indexCount);

    mesh.animNormals = nullptr;
    mesh.animVertices = nullptr;
    mesh.boneIds = nullptr;
    mesh.boneWeights = nullptr;
    mesh.tangents = nullptr;
    mesh.texcoords2 = nullptr;

    logD("index id 0x{:X} vert id 0x{:X}", part.indexBufferID, part.vertexBufferID);

    const auto& indices = m_indexBuffers[part.indexBufferID];
    const auto& vertices = m_vertexBuffers[part.vertexBufferID];
    logD("actual indices size {} vert size {}", indices.size(), vertices.size());
    // memcpy(mesh.indices, indices.data() + part.indexOffset, sizeof(unsigned short) * part.indexCount);
    for (auto i = 0u; i < part.indexCount; i++) {
        // mesh.indices[i] = indices[i];
        mesh.indices[i] = indices[part.indexOffset + i];
        // logD("{}: {}", i, mesh.indices[i]);
    }

    for (auto i = 0u; i < part.vertexCount; i++) {
        auto& vertex = vertices[part.vertexOffset + i];
        mesh.vertices[i * 3 + 0] = vertex.pos.x;
        mesh.vertices[i * 3 + 1] = vertex.pos.y;
        mesh.vertices[i * 3 + 2] = vertex.pos.z;

        mesh.normals[i * 3 + 0] = vertex.normal.x;
        mesh.normals[i * 3 + 1] = vertex.normal.y;
        mesh.normals[i * 3 + 2] = vertex.normal.z;

        mesh.texcoords[i * 2 + 0] = vertex.uv.x;
        mesh.texcoords[i * 2 + 1] = vertex.uv.y;

        mesh.colors[i * 4 + 0] = vertex.color.r;
        mesh.colors[i * 4 + 1] = vertex.color.g;
        mesh.colors[i * 4 + 2] = vertex.color.b;
        mesh.colors[i * 4 + 3] = 255;
        // mesh.colors[i * 4 + 3] = vertex.color.a;
    }

    UploadMesh(&mesh, false);

    auto model = LoadModelFromMesh(mesh);
    m_models.push_back(model);

    logD("Mesh built successfully");

    // trying to make an obj file

    // std::ofstream test("test.obj");

    // for (const auto& v : part.vertices) {
    //     test << fmt::format("v {:.08f} {:.08f} {:.08f}\n", v.pos.x, v.pos.y, v.pos.z);
    //     test << fmt::format("vt {:.08f} {:.08f}\n", v.uv.x, v.uv.y);
    //     test << fmt::format("vn {:.08f} {:.08f} {:.08f}\n", v.normal.x, v.normal.y, v.normal.z);
    // }

    // for (auto i = 0u; i < part.indices.size(); i += 3) {
    //     test << fmt::format("f {0}/{0}/{0} {1}/{1}/{1} {2}/{2}/{2}\n", part.indices[i], part.indices[i+1], part.indices[i+2]);
    // }

    // test.close();
}

void Scene::readPart(BinReader& reader) {
    MeshPart part;

    logD("before loadpart: {:X}", reader.pos());
    loadVertices(reader, part);
    logD("after loadpart: {:X}", reader.pos());

    uint32_t fastBlendVBSSize;
    reader >> fastBlendVBSSize;
    assert(fastBlendVBSSize == 0);

    logD("before loadindices: {:X}", reader.pos());
    loadIndices(reader, part);
    logD("after loadindices: {:X}", reader.pos());

    // skip the remaining part

    reader.skip(4);
    uint32_t skinMtxMapSize;
    reader >> skinMtxMapSize;
    reader.skip(skinMtxMapSize);

    // dynamic parts
    uint32_t dynamicBufferCheck;
    reader >> dynamicBufferCheck;
    assert(dynamicBufferCheck == 0);

    reader.skip(4 + 16 + 16 + 4 + 4);

    // unk VERTEX_SOMETHING
    uint32_t unk;
    reader >> unk;
    assert(unk == 0);

    reader.skip(4 + 4);

    // unk INDICES
    uint32_t unkIndexesUnk;
    reader >> unkIndexesUnk;
    assert(unkIndexesUnk == 0);

    reader.skip(4 + 4 + 4);

    // build a raylib mesh
    genMesh(part);

    m_refCounter++;
}

void Scene::cleanup() {
    for (auto& model : m_models) {
        UnloadModel(model);
    }
}
