#include "Scene.hpp"

#include <sstream>
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
    logD("TXGH: {} textures", texCount);
    reader.skip(texCount * 4 + 4);
    reader >> texCount;
    auto goodTexCount = texCount;
    for (auto i = 0u; i < texCount; i++) {
        reader.skip(16);
        uint32_t nameLen;
        reader >> nameLen;
        if (nameLen > 0) {
            char texName[nameLen];
            reader.read((uint8_t*)texName, nameLen);
            logD("TXGH:   * Texture {}: {}", i, (char*)texName);
        } else {
            logD("TXGH:   * Texture {}: <no name>", i);
            goodTexCount--;
        }
        reader.skip(2 + 1 + 1);
    }
    logD("TXGH: Good texture count: {} (of total {})", goodTexCount, texCount);
    m_refCounter += goodTexCount; // i have 0 idea about how the heck this works but it does
    reader.skip(4);
    uint32_t unk;
    reader >> unk;
    m_refCounter += unk; // ¯\_(ツ)_/¯

    // loading DISP
    auto dispOffset = reader.find(std::string_view("PSID", 4));
    logD("DISP: Found at 0x{:08X}", dispOffset);
    reader.seek(dispOffset);
    reader.skip(4);
    NUEX_ASSERT(reader.read<uint32_t>() == 15) // version should be 15 for now
    reader.skip(reader.read<uint32_t>());      // skip filePath
    reader.skip(4);                            // ROTV

    std::vector<int> commandIndices;
    auto commandsCount = reader.read<uint32_t>();
    for (auto i = 0u; i < commandsCount; i++) {
        auto cmd = reader.read<uint8_t>();
        reader.skip(1);
        auto index = reader.read<uint32_t>();
        // if (cmd == 0xb3) {
        //     logD("DISP: {} {:X} {}", i, cmd, index);
        // }
        commandIndices.push_back(index);
    }

    logD("DISP: {} commandIndices count", commandIndices.size());

    std::unordered_map<int, int> meshMaterials;

    reader.skip(4);
    auto clipObjectsSize = reader.read<uint32_t>();
    logD("DISP: clipObjectsSize = {}", clipObjectsSize);
    for (auto i = 0u; i < clipObjectsSize; i++) {
        reader.skip(2);

        std::vector<int> mtlIndices;
        auto mtlIndicesSize = reader.read<uint32_t>();
        for (auto j = 0u; j < mtlIndicesSize; j++) {
            mtlIndices.push_back(reader.read<uint32_t>());
        }

        // logD("mtlIndices.size = {}", mtlIndices.size());

        auto itemIndicesSize = reader.read<uint32_t>();
        for (auto j = 0u; j < itemIndicesSize; j++) {
            auto cmdIndex = reader.read<uint32_t>();
            auto instanceIndex = commandIndices[cmdIndex];
            // logD("i = {} j = {} index = {} instanceIndex = {} mtlIndex = {}", i, j, cmdIndex, instanceIndex, mtlIndices[j]);
            meshMaterials[instanceIndex] = mtlIndices[j];
        }
    }

    for (const auto [partIdx, mtlIdx] : meshMaterials) {
        logD("DISP: Mesh part {}: material {}", partIdx, mtlIdx);
    }

    // loading UMTL
    auto utmlOffset = reader.find(std::string_view("LTMU", 4));
    logD("UMTL: Found at 0x{:08X}", utmlOffset);
    reader.seek(utmlOffset);
    reader.skip(4);
    uint32_t umtlVer, mtlCount;
    reader >> umtlVer >> mtlCount >> mtlCount; // its intended
    logD("UTML: Version 0x{:X}, {} materials", umtlVer, mtlCount);
    NUEX_ASSERT(umtlVer == 0x94 || umtlVer == 0x95)

    std::vector<int> matTextureIDs;

    for (auto i = 0u; i < mtlCount; i++) {
        logD("UMTL:   * Material {}", i);
        reader.skip(4 * 19 + 1 * 2 + 4 * 10 + 1 * 2 + (4 + 4) * 16 + 1 * 66 + 4 * 4 + (4 + 4 + 1) * 5 + 4 * 5 + 1 * 1);

        int localTIDs[18];
        std::string temp;
        for (auto i = 0u; i < 18; i++) {
            reader >> localTIDs[i];
            temp += std::to_string(localTIDs[i]) + ", ";
        }
        temp.erase(temp.length() - 2);

        matTextureIDs.push_back(localTIDs[0]);

        logD("UMTL:     localTIDs list: [{}]", temp);

        reader.skip(4 * reader.read<uint32_t>() * 3);
        reader.skip(4 * 4 + (1 + 1 + 4 + 4 + 4 + 4) * 4 + 4 * 4 + 1 * 1 + 4 * 54 + 1 + 4 * 3);
        if (umtlVer >= 0x95)
            reader.skip(2);

        auto nameStrLen = reader.read<uint16_t>();
        char matName[nameStrLen];
        reader.read((uint8_t*)matName, nameStrLen);
        logD("UMTL:     Material name: {}", (char*)matName);

        reader.skip(4);
        reader.skip(4 * 20 * 4 + 4 * 20 * 3 * 2);
        reader.skip(reader.read<uint32_t>() * 3);
        reader.skip(reader.read<uint32_t>() * 3);
        reader.skip(4 * 2 + 1 * 3 + 1 * 2 + 1 + 1 * 21);
        reader.skip(4 * 4);

        auto localTID = reader.read<uint32_t>();
        logD("UMTL:     localTID: {}. Should be equal to localTIDs[0]", localTID);

        reader.skip(1 * 2 + 2 * 2 + 1 * 2 + 4 * 6 + 1 * 15 + 4 * 2);

        logD("UMTL:     Material ends at 0x{:08X}", reader.pos());
    }

    for (auto i = 0u; i < matTextureIDs.size(); i++) {
        logD("UMTL: Material {}: Texture ID {}", i, matTextureIDs[i]);
    }

    // loading texture data
    loadTextures(reader, goodTexCount);

    // loading MESH
    auto meshOffset = reader.find(std::string_view("HSEM", 4));
    logD("MESH: Found at 0x{:08X}", meshOffset);
    reader.seek(meshOffset);
    reader.skip(4); // MESH 4cc
    uint32_t ver;
    reader >> ver;
    logD("MESH: Version: 0x{:X}", ver);
    NUEX_ASSERT(ver >= 0x30) reader.skip(4); // ROTV
    uint32_t len;                            // parts count
    reader >> len;
    logD("MESH: {} parts", len);

    for (auto i = 0u; i < len; i++) {
        // logD("======= loading part {} =======", i);
        logD("MESH:   * Part {}", i);
        MeshPart part;
        part.textureID = matTextureIDs[meshMaterials[i]];

        readPart(reader, part);
        genMesh(part);
    }
}

void Scene::loadVertices(BinReader& reader, MeshPart& part) {
    // VERTICES
    uint32_t size;
    reader >> size;
    NUEX_ASSERT(size == 1 || size == 2);

    // VERTEX_SOMETHING
    std::vector<MeshVertex> vertices;
    part.vertexBufferID = m_refCounter;

    for (auto i = 0; i < size; i++) {
        uint32_t unk;
        reader >> unk;
        if (unk >> 3 * 8 == 0xC0) {
            auto id = unk ^ 0xC0'00'00'00;
            logD("MESH:     Reusing vertex buffer 0x{:X}", id);
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
        logD("MESH:     New vertex buffer 0x{:X} of length {}", m_refCounter, count);

        uint32_t nbAttribs;
        reader >> nbAttribs;

        std::vector<MeshAttrib> attribs;

        for (auto i = 0u; i < nbAttribs; i++) {
            MeshAttrib attrib;
            reader >> attrib.valType >> attrib.varType;
            // logD("attrib: {} {}", (int)attrib.valType, (int)attrib.varType);
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
        logD("MESH:     Reusing index buffer 0x{:X}", id);
        part.indexBufferID = id;
        reader.skip(4);
    } else {
        reader.skip(4); // flags
        uint32_t count, size;
        reader >> count >> size;
        logD("MESH:     New index buffer 0x{:X} of length {}", m_refCounter, count);
        NUEX_ASSERT(size == 2);

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
    // logD("indexOffset {}, indexCount {}, verticesOffset {}, verticesCount {}", part.indexOffset, part.indexCount,
    //      part.vertexOffset, part.vertexCount);
}

void Scene::genMesh(const MeshPart& part) {
    Mesh mesh = {0};

    mesh.vertexCount = part.vertexCount;
    mesh.triangleCount = part.indexCount / 3;
    // logD("vertex count {} tri count {}", mesh.vertexCount, mesh.triangleCount);

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

    // logD("index id 0x{:X} vert id 0x{:X}", part.indexBufferID, part.vertexBufferID);

    const auto& indices = m_indexBuffers[part.indexBufferID];
    const auto& vertices = m_vertexBuffers[part.vertexBufferID];
    // logD("actual indices size {} vert size {}", indices.size(), vertices.size());
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
    logD("MESH:     Part texture id: {}", part.textureID);
    // auto x = LoadTexture("C:\\Dev\\MCRewrite\\decomps\\alpha\\c0.30_01c\\decomp\\dirt.png");
    if (part.textureID != -1 && m_textures.contains(part.textureID)) {
        logD("MESH:     Applying texture: {}", part.textureID);
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = m_textures[part.textureID];
    }
    m_models.push_back(model);

    logD("MESH:     Mesh built successfully");
}

void Scene::readPart(BinReader& reader, MeshPart& part) {
    // logD("before loadpart: {:X}", reader.pos());
    loadVertices(reader, part);
    // logD("after loadpart: {:X}", reader.pos());

    uint32_t fastBlendVBSSize;
    reader >> fastBlendVBSSize;
    NUEX_ASSERT(fastBlendVBSSize == 0);

    // logD("before loadindices: {:X}", reader.pos());
    loadIndices(reader, part);
    // logD("after loadindices: {:X}", reader.pos());

    // skip the remaining part

    reader.skip(4);
    uint32_t skinMtxMapSize;
    reader >> skinMtxMapSize;
    reader.skip(skinMtxMapSize);

    // dynamic parts
    uint32_t dynamicBufferCheck;
    reader >> dynamicBufferCheck;
    NUEX_ASSERT(dynamicBufferCheck == 0);

    reader.skip(4 + 16 + 16 + 4 + 4);

    // unk VERTEX_SOMETHING
    uint32_t unk;
    reader >> unk;
    NUEX_ASSERT(unk == 0);

    reader.skip(4 + 4);

    // unk INDICES
    uint32_t unkIndexesUnk;
    reader >> unkIndexesUnk;
    NUEX_ASSERT(unkIndexesUnk == 0);

    reader.skip(4 + 4 + 4);

    m_refCounter++;
}

void Scene::loadTextures(BinReader& reader, int count) {
    auto firstTex = reader.find(std::string_view("DDS ", 4));
    logD("TEXTURES: Textures start address found at 0x{:08X}", firstTex);
    reader.seek(firstTex);

    reader.setEndianness(Endianness::Little);

    for (auto i = 0u; i < count; i++) {
        auto startPos = reader.pos();
        logD("TEXTURES:   * Loading texture {} at 0x{:08X}", i, startPos);

        auto dataLen = 0u;

        bool skip = false;

        reader.seek(startPos + 84);
        auto type = reader.read<uint32_t>();
        switch (type) {
        case 0x31'54'58'44: { // DXT1
            logD("TEXTURES:     Texture type: DXT1");
            int num1, num2;
            reader.seek(startPos + 12);
            reader >> num1 >> num2;
            reader.seek(startPos + 28);
            int int32_1 = reader.read<uint32_t>();
            reader.seek(startPos + 112);
            int int32_2 = reader.read<uint32_t>();
            logD("TEXTURES:     Texture info: {}x{}, {} mips, cubeMapFlags: {:08X}", num2, num1, int32_1, int32_2);
            int num3 = num1 * num2 >> 1;
            for (int index = 1; index < int32_1; ++index) {
                num1 = num1 >> 1 < 4 ? 4 : num1 >> 1;
                num2 = num2 >> 1 < 4 ? 4 : num2 >> 1;
                num3 += num1 * num2 >> 1;
            }
            if (int32_2 != 0)
                num3 *= 6;
            num3 += 128;
            dataLen = num3;
        } break;
        case 0x35'54'58'44: { // DXT5
            logD("TEXTURES:     Texture type: DXT5");
            reader.seek(startPos + 12);
            int num1, num2;
            reader >> num1 >> num2;
            reader.seek(startPos + 28);
            int int32_1 = reader.read<uint32_t>();
            reader.seek(startPos + 112);
            int int32_2 = reader.read<uint32_t>();
            logD("TEXTURES:     Texture info: {}x{}, {} mips, cubeMapFlags: {:08X}", num2, num1, int32_1, int32_2);
            int num3 = num1 * num2;
            for (int index = 1; index < int32_1; ++index) {
                num1 = num1 >> 1 < 4 ? 4 : num1 >> 1;
                num2 = num2 >> 1 < 4 ? 4 : num2 >> 1;
                num3 += num1 * num2;
            }
            if (int32_2 != 0)
                num3 *= 6;
            num3 += 128;
            dataLen = num3;
        } break;
        case 116: { // D3D
            logD("TEXTURES:     Texture type: D3D");
            reader.seek(startPos + 12);
            int int32_1, int32_2;
            reader >> int32_1 >> int32_2;
            reader.seek(startPos + 28);
            int int32_3 = reader.read<uint32_t>();
            logD("TEXTURES:     Texture info: {}x{}, {} mips", int32_2, int32_1, int32_3);
            if (int32_3 != 1) {
                logW("TEXTURES:     D3D mipmaps = {}, skipping!", int32_3);
                skip = true;
            }
            int dformat74FileSize = int32_1 * int32_2 * 16 + 128;
            dataLen = dformat74FileSize;
        } break;
        default:
            logE("TEXTURES:     Unknown texture type 0x{:08X}", type);
            // exit(1);
        }

        logD("TEXTURES:     Texture data length: 0x{:08X}", dataLen);

        reader.seek(startPos);

        // load tex here

        auto data = std::make_unique<uint8_t[]>(dataLen);
        reader.read(data.get(), dataLen);

        reader.seek(startPos + dataLen);

        if (skip)
            continue;

        ////////////////

        // dds::Image img;
        // auto res = dds::readFile("C:\\Dev\\Cpp\\NuExplorer\\build\\tex24.dds", &img);
        // if (res != dds::ReadResult::Success) {
        //     logE("Failed to load a texture!!");
        //     continue;
        // }

        // logD("{:08X}", img.data.size());

        // Image rlImg;
        // rlImg.width = img.width;
        // rlImg.height = img.height;
        // rlImg.mipmaps = img.numMips;
        // switch (img.format) {
        // case DXGI_FORMAT_BC1_UNORM:
        //     rlImg.format = PIXELFORMAT_COMPRESSED_DXT1_RGB;
        //     break;
        // case DXGI_FORMAT_BC2_UNORM:
        //     rlImg.format = PIXELFORMAT_COMPRESSED_DXT3_RGBA;
        //     break;
        // case DXGI_FORMAT_BC3_UNORM:
        //     rlImg.format = PIXELFORMAT_COMPRESSED_DXT5_RGBA;
        //     break;
        // case DXGI_FORMAT_R32G32B32A32_FLOAT:
        //     rlImg.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
        //     break;
        // default:
        //     logE("NOT HANDLED TETXURE FORMAT!!!!!!!!!");
        //     logD("img fmt {}", (int)img.format);
        //     break;
        // }
        // rlImg.data = RL_MALLOC(img.data.size() * sizeof(uint8_t));
        // memcpy(rlImg.data, img.data.data(), img.data.size());

        // auto rlTex = LoadTextureFromImage(rlImg);
        // m_textures[i] = rlTex;

        // UnloadImage(rlImg);

        //////////////////

        // auto fstr = std::ofstream(fmt::format("tex{}.dds", i), std::ios::binary | std::ios::out);
        // fstr.write((const char*)data.get(), dataLen);
        // fstr.close();

        // logD("datalen {}", dataLen);

        // std::stringstream texStream;
        // texStream.write((char*)data.get(), dataLen);

        // nv_dds::CDDSImage img;
        // img.load(texStream);

        // auto valid = img.is_valid();
        // logD("img valid {} pixels 0x{:08X} compressed {}", valid, (uintptr_t)((uint8_t*)img), img.is_compressed());

        // auto ptr = (uint8_t*)img;
        // for (auto i = ptr; i < ptr + 64; i++) {
        //     logD("{:02X}", *i);
        // }

        // Image rlImg;
        // rlImg.format = PixelFormat::PIXELFORMAT_COMPRESSED_DXT1_RGBA;
        // rlImg.width = img.get_width();
        // rlImg.height = img.get_height();
        // rlImg.mipmaps = img.get_num_mipmaps();
        // rlImg.data = (uint8_t*)img;

        // auto rlTex = LoadTextureFromImage(rlImg);

        // img.load()

        // int x, y, comp;
        // auto buf = stbi_load_from_memory(data.get(), dataLen, &x, &y, &comp, 0);
        // logD("buf {:X}", (uintptr_t)buf);
        // // delete buf;
        // stbi_image_free(buf);

        // stbi_load_from_memory()

        // logD("x");

        // auto img = LoadImageFromMemory(".dds", data.get(), dataLen);
        // logD("x");
        // auto tex = LoadTextureFromImage(img);
        // // logD("x");
        // m_textures.push_back(tex);

        // dds::Image img;
        // auto result =
        // dds::readFile("C:\\Dev\\LEGO\\lotr\\extracted\\LEVELS\\1_FOTR\\1_1PROLOGUE\\1_1PROLOGUEA\\1_1PROLOGUEA_NXG.GSC_tex\\tex11.dds",
        // &img); logD("img {}x{}", img.width, img.height); img.data
        
        // m_textures[i] = LoadTexture("C:\\Dev\\LEGO\\lotr\\extracted\\LEVELS\\1_FOTR\\1_1PROLOGUE\\1_1PROLOGUEA\\1_1PROLOGUEA_NXG.GSC_tex\\tex24_orig.dds");
        auto img = LoadImageFromMemory(".dds", data.get(), dataLen);
        auto tex = LoadTextureFromImage(img);
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
        m_textures[i] = tex;
    }

    reader.setEndianness(Endianness::Big);
}

void Scene::cleanup() {
    for (auto& model : m_models) {
        UnloadModel(model);
    }

    for (auto& [i, tex] : m_textures) {
        UnloadTexture(tex);
    }
}
