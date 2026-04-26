#ifndef TERRAIN_BIOMES_H
#define TERRAIN_BIOMES_H

#include "../terrain.h"
#include "fields.h"

#include <cstdint>

namespace terrain {

struct BiomeColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

void computeBiomeFields(TerrainFields& fields);
void computeBiomeFieldsWFC(TerrainFields& fields, const TerrainSettings& settings);

const char* biomeName(BiomeId biome);
BiomeColor biomeColor(BiomeId biome);

const char* ecologyName(EcologyId ecology);
BiomeColor ecologyColor(EcologyId ecology);

const char* landformName(LandformId landform);
BiomeColor landformColor(LandformId landform);

} // namespace terrain

#endif // TERRAIN_BIOMES_H
