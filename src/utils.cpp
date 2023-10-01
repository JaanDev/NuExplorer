#include "utils.hpp"

namespace utils {
    float getMiniFloat(unsigned char val) {
        static bool setLookUp = false;
        static float lookUp[256];

        if (!setLookUp) {
            // https://github.com/JamesFrancoe/TTGames-Extraction-Tools/blob/main/ExtractNgxMESH/ExtractNxgMESH.MESHs/MESH04.cs#L24
            double num = 0.007874015748031496;
            lookUp[0] = -1.f;

            for (auto i = 1u; i < 256; i++) {
                lookUp[i] = (float)((double)lookUp[i - 1] + num);
            }

            lookUp[127] = 0.f;
            lookUp[255] = 1.f;

            setLookUp = true;
        }

        return lookUp[val];
    }

    size_t getVarSize(MeshVarType type) {
        auto val = (uint8_t)type;
        return ((val > 4) ? (4 * (val == 6) + 4) : (val * 4));
    }
} // namespace utils
