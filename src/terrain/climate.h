#ifndef TERRAIN_CLIMATE_H
#define TERRAIN_CLIMATE_H

#include "fields.h"
#include "../terrain.h"

#include <functional>

namespace terrain
{

struct ClimateNoiseInput
{
    float sampleX = 0.0f;
    float sampleZ = 0.0f;
    float elevationNorm = 0.0f;
    float slope = 0.0f;
    float riverDistance = 0.0f;
    float riverWeight = 0.0f;
    float mountainWeight = 0.0f;
    float latitude = 0.5f;
    float baseFrequency = 0.007f;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    ClimateSettings settings;
    std::function<float(float, float, int, float, float)> fbm;
};

struct ClimateSample
{
    float temperature = 0.5f;
    float precipitation = 0.5f;
    float moisture = 0.5f;
};

ClimateSample computeClimateSample(const ClimateNoiseInput& in);

void computeClimateFields(
    TerrainFields& fields,
    const TerrainSettings& terrainSettings,
    std::function<float(float, float, int, float, float)> fbm);

} // namespace terrain

#endif // TERRAIN_CLIMATE_H
