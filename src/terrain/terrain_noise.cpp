#include "terrain_noise.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

constexpr float kSqrt3 = 1.7320508075688772f;
constexpr float kF2 = 0.5f * (kSqrt3 - 1.0f);
constexpr float kG2 = (3.0f - kSqrt3) / 6.0f;

int fastFloor(float x) {
    int x_int = static_cast<int>(x);
    return (x >= 0.0f) ? x_int : x_int - 1;
}

} // namespace

float NoiseContext::simplex2D(float x, float y) const {
    static constexpr float grads[8][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        {0.7071f, 0.7071f}, {-0.7071f, 0.7071f}, {0.7071f, -0.7071f}, {-0.7071f, -0.7071f}
    };

    const float s = (x + y) * kF2;
    const int i = fastFloor(x + s);
    const int j = fastFloor(y + s);

    const float t = static_cast<float>(i + j) * kG2;
    const float x0 = x - (static_cast<float>(i) - t);
    const float y0 = y - (static_cast<float>(j) - t);

    int i1 = (x0 > y0) ? 1 : 0;
    int j1 = (x0 > y0) ? 0 : 1;

    const float x1 = x0 - static_cast<float>(i1) + kG2;
    const float y1 = y0 - static_cast<float>(j1) + kG2;
    const float x2 = x0 - 1.0f + 2.0f * kG2;
    const float y2 = y0 - 1.0f + 2.0f * kG2;

    const int ii = i & 255;
    const int jj = j & 255;

    const int gi0 = permutation[ii + permutation[jj]] & 7;
    const int gi1 = permutation[ii + i1 + permutation[jj + j1]] & 7;
    const int gi2 = permutation[ii + 1 + permutation[jj + 1]] & 7;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0.0f) { t0 *= t0; n0 = t0 * t0 * (grads[gi0][0] * x0 + grads[gi0][1] * y0); }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f) { t1 *= t1; n1 = t1 * t1 * (grads[gi1][0] * x1 + grads[gi1][1] * y1); }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f) { t2 *= t2; n2 = t2 * t2 * (grads[gi2][0] * x2 + grads[gi2][1] * y2); }

    return 70.0f * (n0 + n1 + n2);
}

float NoiseContext::perlin2D(float x, float y) const {
    static constexpr float grads[8][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        {0.70710678f, 0.70710678f}, {-0.70710678f, 0.70710678f},
        {0.70710678f, -0.70710678f}, {-0.70710678f, -0.70710678f}
    };

    const int X = fastFloor(x) & 255;
    const int Y = fastFloor(y) & 255;

    const float xf = x - fastFloor(x);
    const float yf = y - fastFloor(y);

    const int gi00 = permutation[permutation[X] + Y] & 7;
    const int gi10 = permutation[permutation[X + 1] + Y] & 7;
    const int gi01 = permutation[permutation[X] + Y + 1] & 7;
    const int gi11 = permutation[permutation[X + 1] + Y + 1] & 7;

    auto dotGrad = [&](int gi, float dx, float dy) {
        return grads[gi][0] * dx + grads[gi][1] * dy;
    };

    float n00 = dotGrad(gi00, xf, yf);
    float n10 = dotGrad(gi10, xf - 1.0f, yf);
    float n01 = dotGrad(gi01, xf, yf - 1.0f);
    float n11 = dotGrad(gi11, xf - 1.0f, yf - 1.0f);

    const float u = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
    const float v = yf * yf * yf * (yf * (yf * 6.0f - 15.0f) + 10.0f);

    const float nx0 = n00 + u * (n10 - n00);
    const float nx1 = n01 + u * (n11 - n01);

    return nx0 + v * (nx1 - nx0);
}

float NoiseContext::perlinFbm(float x, float y, int octaves, float lacunarity, float gain, float frequency) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float freq = frequency;
    float amplitudeSum = 0.0f;
    for (int octave = 0; octave < octaves; ++octave) {
        const float n = perlin2D(x * freq, y * freq);
        value += amplitude * n;
        amplitudeSum += amplitude;
        amplitude *= gain;
        freq *= lacunarity;
    }
    return amplitudeSum > 0.0f ? value / amplitudeSum : 0.0f;
}

float NoiseContext::fbm(float x, float y, int octaves, float lacunarity, float gain, float frequency) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, frequency, [](float n) { return n; });
}

float NoiseContext::ridgedFbm(float x, float y, int octaves, float lacunarity, float gain, float sharpness, float frequency) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, frequency,
        [sharpness](float n) { return std::pow(std::clamp(1.0f - std::fabs(n), 0.0f, 1.0f), sharpness); });
}

} // namespace terrain
