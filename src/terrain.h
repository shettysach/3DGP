#ifndef TERRAIN_H
#define TERRAIN_H

#include <cstdint>
#include <vector>

namespace terrain {

enum class LandformId : uint8_t {
    Lowland = 0,
    Plain,
    Valley,
    Foothill,
    Mountain,
    Alpine,
    Snowcap,
    Count,
};

enum class EcologyId : uint8_t {
    Desert = 0,
    Steppe,
    Grassland,
    Forest,
    Taiga,
    Tundra,
    Marsh,
    Count,
};

enum class BiomeId : uint8_t {
    MarshLowland = 0,
    DesertPlain,
    SteppePlain,
    GrasslandPlain,
    ForestPlain,
    TaigaPlain,
    TundraPlain,
    SteppeFoothill,
    GrasslandFoothill,
    ForestFoothill,
    TaigaFoothill,
    RockyAlpine,
    Alpine,
    Snow,
    Count,
};

struct NoiseSettings {
    float frequency = 0.007f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeSharpness = 2.0f;
    float warpFrequency = 0.003f;
    float warpAmplitude = 45.0f;
};

struct RiverSettings {
    float sourceDensity = 0.00015f;
    float minSourceHeight = 0.50f;
    float sourceAccumulation = 100.0f;
    float mainAccumulation = 220.0f;
    int minSourceSeparation = 18;
    int maxHalfWidth = 2;
    float baseCarveFraction = 0.02f;
    float maxCarveFraction = 0.07f;
    float bankFalloff = 1.8f;
    float coreThreshold = 0.55f;
};

struct ClimateSettings {
    float temperatureFrequency = 0.0007f;
    int temperatureOctaves = 3;
    float precipitationFrequency = 0.0005f;
    int precipitationOctaves = 4;
    float moistureFrequency = 0.0009f;
    int moistureOctaves = 2;
    float latitudeStrength = 0.20f;
    float temperatureLapseRate = 0.55f;
    float orographicPrecipitationStrength = 0.12f;
    int riverMoistureRadius = 18;
    float riverMoistureStrength = 0.24f;
    float slopeDryingStrength = 0.16f;
    float temperatureDryingStrength = 0.10f;
};

struct TerrainSettings {
    int width = 200;
    int depth = 200;
    float horizontalScale = 2.0f;
    float verticalScale = 80.0f;
    bool islandFalloff = true;
    float falloffRadius = 0.9f;
    float falloffPower = 2.2f;
    uint32_t seed = 1234u;
    NoiseSettings noise;
    RiverSettings rivers;
    ClimateSettings climate;
};

struct TerrainVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float slope = 0.0f;
    float mountainWeight = 0.0f;
    float plainsWeight = 1.0f;
    float riverWeight = 0.0f;
    float temperature = 0.5f;
    float precipitation = 0.5f;
    float moisture = 0.5f;
    uint8_t landform = static_cast<uint8_t>(LandformId::Plain);
    uint8_t ecology = static_cast<uint8_t>(EcologyId::Grassland);
    uint8_t primaryBiome = static_cast<uint8_t>(BiomeId::GrasslandPlain);
    uint8_t secondaryBiome = static_cast<uint8_t>(BiomeId::GrasslandPlain);
    float primaryBiomeWeight = 1.0f;
    float secondaryBiomeWeight = 0.0f;
};

struct TerrainMesh {
    int width = 0;
    int depth = 0;
    float horizontalScale = 1.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    std::vector<float> heights;
    std::vector<float> temperatureMap;
    std::vector<float> precipitationMap;
    std::vector<float> moistureMap;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

class TerrainGenerator {
  public:
    explicit TerrainGenerator(TerrainSettings settings = {});

    void setSettings(const TerrainSettings& settings);
    const TerrainSettings& settings() const;

    TerrainMesh generateMesh() const;

  private:
    TerrainSettings settings_;
    std::vector<int> permutation_;

    void reseed(uint32_t seed);
    float simplexNoise2D(float x, float y) const;
    float fractalBrownianMotion(float x, float y, int octaves, float lacunarity, float gain) const;
    float ridgedFbm(
        float x,
        float y,
        int octaves,
        float lacunarity,
        float gain,
        float sharpness) const;

    template <typename F>
    float octaveNoise(float x, float y, int octaves, float lacunarity, float gain, F transform) const {
        float value = 0.0f;
        float amplitude = 1.0f;
        float frequency = settings_.noise.frequency;
        float amplitudeSum = 0.0f;
        for (int octave = 0; octave < octaves; ++octave) {
            const float n = simplexNoise2D(x * frequency, y * frequency);
            value += amplitude * transform(n);
            amplitudeSum += amplitude;
            amplitude *= gain;
            frequency *= lacunarity;
        }
        return amplitudeSum > 0.0f ? value / amplitudeSum : 0.0f;
    }
};

} // namespace terrain

#endif // TERRAIN_H
