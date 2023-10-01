#pragma once
#include <cstdint>

enum class MeshVarType : uint8_t {
    Vec2f = 2,
    Vec3f = 3,
    Vec4f = 4,
    Vec2half = 5,
    Vec4half = 6,
    Vec4char = 7,
    Vec4mini = 8,
    Col4char = 9
};

enum class MeshValType : uint8_t {
    Position = 0,
    Normal = 1,
    ColorSet0 = 2,
    Tangent = 3,
    ColorSet1 = 4,
    UVSet1 = 5,
    Unknown = 6,
    UVSet2 = 7,
    Unknown2 = 8,
    BlendIndices = 9,
    BlendWeight = 10,
    Unknown3 = 11,
    LightDirSet = 12,
    LightColSet = 13
};

namespace utils {
    float getMiniFloat(unsigned char val);
    size_t getVarSize(MeshVarType type);
}
