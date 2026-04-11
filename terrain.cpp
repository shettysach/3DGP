#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace terrain
{

ScalarField3D::ScalarField3D(int width, int height, int depth)
    : width_(width), height_(height), depth_(depth),
      data_(static_cast<size_t>(width) * static_cast<size_t>(height) *
                static_cast<size_t>(depth),
            0.0f)
{
    if (width <= 0 || height <= 0 || depth <= 0)
    {
        throw std::invalid_argument("ScalarField3D dimensions must be positive");
    }
}

float ScalarField3D::get(int x, int y, int z) const
{
    return data_.at(indexOf(x, y, z));
}

void ScalarField3D::set(int x, int y, int z, float value)
{
    data_.at(indexOf(x, y, z)) = value;
}

size_t ScalarField3D::indexOf(int x, int y, int z) const
{
    if (x < 0 || x >= width_ || y < 0 || y >= height_ || z < 0 || z >= depth_)
    {
        throw std::out_of_range("ScalarField3D index out of range");
    }
    const size_t ix = static_cast<size_t>(x);
    const size_t iy = static_cast<size_t>(y);
    const size_t iz = static_cast<size_t>(z);
    const size_t w = static_cast<size_t>(width_);
    const size_t h = static_cast<size_t>(height_);
    return ix + w * (iy + h * iz);
}

class Perlin3D
{
  public:
    explicit Perlin3D(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed)
    {
        std::vector<int> p(256);
        std::iota(p.begin(), p.end(), 0);

        std::mt19937 rng(seed);
        std::shuffle(p.begin(), p.end(), rng);

        permutation_.resize(512);
        for (int i = 0; i < 512; ++i)
        {
            permutation_[i] = p[i & 255];
        }
    }

    float noise(float x, float y, float z) const
    {
        const int xi = static_cast<int>(std::floor(x)) & 255;
        const int yi = static_cast<int>(std::floor(y)) & 255;
        const int zi = static_cast<int>(std::floor(z)) & 255;

        const float xf = x - std::floor(x);
        const float yf = y - std::floor(y);
        const float zf = z - std::floor(z);

        const float u = fade(xf);
        const float v = fade(yf);
        const float w = fade(zf);

        const int aaa = permutation_[permutation_[permutation_[xi] + yi] + zi];
        const int aba = permutation_[permutation_[permutation_[xi] + yi + 1] + zi];
        const int aab = permutation_[permutation_[permutation_[xi] + yi] + zi + 1];
        const int abb = permutation_[permutation_[permutation_[xi] + yi + 1] + zi + 1];
        const int baa = permutation_[permutation_[permutation_[xi + 1] + yi] + zi];
        const int bba = permutation_[permutation_[permutation_[xi + 1] + yi + 1] + zi];
        const int bab = permutation_[permutation_[permutation_[xi + 1] + yi] + zi + 1];
        const int bbb = permutation_[permutation_[permutation_[xi + 1] + yi + 1] + zi + 1];

        const float x1 = lerp(dotGrad(aaa, xf, yf, zf),
                              dotGrad(baa, xf - 1.0f, yf, zf), u);
        const float x2 = lerp(dotGrad(aba, xf, yf - 1.0f, zf),
                              dotGrad(bba, xf - 1.0f, yf - 1.0f, zf), u);
        const float y1 = lerp(x1, x2, v);

        const float x3 = lerp(dotGrad(aab, xf, yf, zf - 1.0f),
                              dotGrad(bab, xf - 1.0f, yf, zf - 1.0f), u);
        const float x4 = lerp(dotGrad(abb, xf, yf - 1.0f, zf - 1.0f),
                              dotGrad(bbb, xf - 1.0f, yf - 1.0f, zf - 1.0f), u);
        const float y2 = lerp(x3, x4, v);

        return lerp(y1, y2, w);
    }

  private:
    std::vector<int> permutation_;

    static float fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    static float dotGrad(int hash, float x, float y, float z)
    {
        // Hardcode for now. generate later ?
        static constexpr float gradients[12][3] = {
            {1.0f, 1.0f, 0.0f}, //
            {-1.0f, 1.0f, 0.0f},
            {1.0f, -1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},
            {1.0f, 0.0f, 1.0f},
            {-1.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, -1.0f},
            {-1.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 1.0f},
            {0.0f, -1.0f, 1.0f},
            {0.0f, 1.0f, -1.0f},
            {0.0f, -1.0f, -1.0f}};

        const float* g = gradients[hash % 12];
        return g[0] * x + g[1] * y + g[2] * z;
    }
};

namespace
{
Perlin3D g_perlin(1337u);
NoiseSettings g_noiseSettings;
} // namespace

void configureNoise(const NoiseSettings& settings, uint32_t seed)
{
    g_noiseSettings = settings;
    g_perlin.reseed(seed);
}

float perlinNoise(float x, float y, float z)
{
    return g_perlin.noise(x, y, z);
}

float fbm(float x, float y, float z)
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = g_noiseSettings.frequency;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < g_noiseSettings.octaves; ++octave)
    {
        value += amplitude * perlinNoise(x * frequency, y * frequency, z * frequency);
        amplitudeSum += amplitude;
        amplitude *= g_noiseSettings.gain;
        frequency *= g_noiseSettings.lacunarity;
    }

    if (amplitudeSum == 0.0f)
    {
        return 0.0f;
    }
    return value / amplitudeSum;
}

float density(float x, float y, float z)
{
    const float noise = fbm(x, y, z);
    const float normalized = 0.5f * noise + 0.5f;
    return y - normalized * g_noiseSettings.heightScale;
}

void populateTerrain(ScalarField3D& field, float yScale)
{
    for (int z = 0; z < field.depth(); ++z)
    {
        for (int y = 0; y < field.height(); ++y)
        {
            for (int x = 0; x < field.width(); ++x)
            {
                const float worldX = static_cast<float>(x);
                const float worldY = static_cast<float>(y) * yScale;
                const float worldZ = static_cast<float>(z);
                field.set(x, y, z, density(worldX, worldY, worldZ));
            }
        }
    }
}

} // namespace terrain
