#ifndef TERRAIN_H
#define TERRAIN_H

#include <cstdint>
#include <vector>

namespace terrain
{

struct NoiseSettings
{
    float frequency = 0.007f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float ridgeSharpness = 2.0f;
    float warpFrequency = 0.003f;
    float warpAmplitude = 45.0f;
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
    uint32_t seed = 1234u;
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
    float mountainWeight = 0.0f;
    float plainsWeight = 1.0f;
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
    float fractalBrownianMotion(float x, float y, int octaves, float lacunarity, float gain) const;
    float ridgedFbm(
        float x,
        float y,
        int octaves,
        float lacunarity,
        float gain,
        float sharpness) const;

    template <typename F>
    float octaveNoise(float x, float y, int octaves, float lacunarity, float gain, F transform) const
    {
        float value = 0.0f;
        float amplitude = 1.0f;
        float frequency = settings_.noise.frequency;
        float amplitudeSum = 0.0f;
        for (int octave = 0; octave < octaves; ++octave)
        {
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
