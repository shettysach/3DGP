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
    static constexpr float grads[4][2] = {
        {1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}
    };

    const int xi0 = fastFloor(x);
    const int yi0 = fastFloor(y);
    const float xf0 = x - static_cast<float>(xi0);
    const float yf0 = y - static_cast<float>(yi0);
    const float xf1 = xf0 - 1.0f;
    const float yf1 = yf0 - 1.0f;

    const int xi = xi0 & 255;
    const int yi = yi0 & 255;

    const float u = xf0 * xf0 * xf0 * (xf0 * (xf0 * 6.0f - 15.0f) + 10.0f);
    const float v = yf0 * yf0 * yf0 * (yf0 * (yf0 * 6.0f - 15.0f) + 10.0f);

    const int gi00 = permutation[xi + permutation[yi]] & 3;
    const int gi10 = permutation[xi + 1 + permutation[yi]] & 3;
    const int gi01 = permutation[xi + permutation[yi + 1]] & 3;
    const int gi11 = permutation[xi + 1 + permutation[yi + 1]] & 3;

    const float n00 = grads[gi00][0] * xf0 + grads[gi00][1] * yf0;
    const float n10 = grads[gi10][0] * xf1 + grads[gi10][1] * yf0;
    const float n01 = grads[gi01][0] * xf0 + grads[gi01][1] * yf1;
    const float n11 = grads[gi11][0] * xf1 + grads[gi11][1] * yf1;

    const float x0 = n00 + u * (n10 - n00);
    const float x1 = n01 + u * (n11 - n01);
    return x0 + v * (x1 - x0);
}

float NoiseContext::fbm(float x, float y, int octaves, float lacunarity, float gain, float frequency) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, frequency, [](float n) { return n; });
}

float NoiseContext::ridgedFbm(float x, float y, int octaves, float lacunarity, float gain, float sharpness, float frequency) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, frequency,
        [sharpness](float n) { return std::pow(std::clamp(1.0f - std::fabs(n), 0.0f, 1.0f), sharpness); });
}

} // namespace terrain
