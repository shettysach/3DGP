#ifndef TERRAIN_NOISE_H
#define TERRAIN_NOISE_H

#include <cstdint>
#include <vector>

namespace terrain {

struct NoiseContext {
    std::vector<int> permutation;

    float simplex2D(float x, float y) const;

    float fbm(float x, float y, int octaves, float lacunarity, float gain, float frequency) const;
    float ridgedFbm(float x, float y, int octaves, float lacunarity, float gain, float sharpness, float frequency) const;

    template <typename F>
    float octaveNoise(float x, float y, int octaves, float lacunarity, float gain, float frequency, F transform) const {
        float value = 0.0f;
        float amplitude = 1.0f;
        float freq = frequency;
        float amplitudeSum = 0.0f;
        for (int octave = 0; octave < octaves; ++octave) {
            const float n = simplex2D(x * freq, y * freq);
            value += amplitude * transform(n);
            amplitudeSum += amplitude;
            amplitude *= gain;
            freq *= lacunarity;
        }
        return amplitudeSum > 0.0f ? value / amplitudeSum : 0.0f;
    }
};

} // namespace terrain

#endif // TERRAIN_NOISE_H
