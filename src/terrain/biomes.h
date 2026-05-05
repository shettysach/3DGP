#ifndef TERRAIN_BIOMES_H
#define TERRAIN_BIOMES_H

#include <cstdint>

#include "../terrain.h"
#include "fields.h"

namespace terrain {

struct BiomeColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

void computeBiomeFieldsWFC(
    TerrainFields& fields,
    const TerrainSettings& settings
);

const char* biomeName(BiomeId biome);
BiomeColor biomeColor(BiomeId biome);

} // namespace terrain

#endif // TERRAIN_BIOMES_H
