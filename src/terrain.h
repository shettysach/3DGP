#ifndef TERRAIN_H
#define TERRAIN_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace terrain
{

struct NoiseSettings
{
    float frequency = 0.015f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeSharpness = 2.4f;
    float warpFrequency = 0.006f;
    float warpAmplitude = 28.0f;
};

struct TerrainSettings
{
    int width = 257;
    int depth = 257;
    float horizontalScale = 2.0f;
    float verticalScale = 80.0f;
    bool islandFalloff = true;
    float falloffRadius = 0.9f;
    float falloffPower = 2.2f;
    uint32_t seed = 2026u;
    NoiseSettings noise;
};

struct TerrainVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
};

struct TerrainMesh
{
    int width = 0;
    int depth = 0;
    float horizontalScale = 1.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    std::vector<float> heights;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

class TerrainGenerator
{
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
    float fbm(float x, float y, int octaves, float lacunarity, float gain) const;
    float ridgedFbm(
        float x,
        float y,
        int octaves,
        float lacunarity,
        float gain,
        float sharpness) const;
};

} // namespace terrain

#endif // TERRAIN_H
