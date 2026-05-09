#ifndef TERRAIN_IMAGE_UTIL_H
#define TERRAIN_IMAGE_UTIL_H

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "biomes.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace terrain {

inline void savePNG(const std::string& filename, int width, int depth, const std::vector<BiomeColor>& colors) {
    std::vector<uint8_t> data;
    data.reserve(colors.size() * 3);

    for (const auto& c : colors) {
        data.push_back(static_cast<uint8_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f)));
        data.push_back(static_cast<uint8_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f)));
        data.push_back(static_cast<uint8_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f)));
    }

    stbi_write_png(filename.c_str(), width, depth, 3, data.data(), width * 3);
}

} // namespace terrain

#endif // TERRAIN_IMAGE_UTIL_H
