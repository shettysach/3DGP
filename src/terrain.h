#ifndef TERRAIN_H
#define TERRAIN_H

#include "terrain/terrain_noise.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace graph {
struct CompiledGraph;
}

namespace terrain {

struct TerrainFields;

enum class LandformId : uint8_t {
    Lowland = 0,
    Plain,
    Valley,
    Plateau,
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
    DesertPlateau,
    SteppePlateau,
    GrasslandPlateau,
    ForestPlateau,
    TaigaPlateau,
    TundraPlateau,
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

struct PlateauSettings {
    float frequency = 0.028f;
    float heightScale = 0.42f;
    float steepness = 0.30f;
    float detailAmount = 0.04f;
};

struct TerrainSettings {
    int width = 512;
    int depth = 512;
    float horizontalScale = 2.0f;
    float verticalScale = 80.0f;
    bool islandFalloff = true;
    float falloffRadius = 0.9f;
    float falloffPower = 2.2f;
    uint32_t seed = 1234u;
    bool useWFC = true;
    float voronoiCellSize = 24.0f;
    NoiseSettings noise;
    RiverSettings rivers;
    ClimateSettings climate;
    PlateauSettings plateaus;
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
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

class TerrainGenerator {
  public:
    explicit TerrainGenerator(TerrainSettings settings = {});

    void setSettings(const TerrainSettings& settings);
    const TerrainSettings& settings() const;

    void setBaseGraph(std::shared_ptr<const graph::CompiledGraph> graph);
    const graph::CompiledGraph* baseGraph() const;

    TerrainMesh generateMesh() const;

  private:
    TerrainSettings settings_;
    NoiseContext noiseContext_;
    std::shared_ptr<const graph::CompiledGraph> baseGraph_;

    TerrainFields buildBaseTerrainFields() const;
    void computeClimateFields(TerrainFields& fields) const;

    void reseed(uint32_t seed);
};

} // namespace terrain

#endif // TERRAIN_H
