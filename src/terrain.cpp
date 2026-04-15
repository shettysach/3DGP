#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

namespace terrain
{

namespace
{

constexpr float kSqrt3 = 1.7320508075688772f;
constexpr float kF2 = 0.5f * (kSqrt3 - 1.0f);
constexpr float kG2 = (3.0f - kSqrt3) / 6.0f;

int fastFloor(float x)
{
    return (x >= 0.0f) ? static_cast<int>(x) : static_cast<int>(x) - 1;
}

float clamp01(float x)
{
    return std::max(0.0f, std::min(1.0f, x));
}

} // namespace
void TerrainGenerator::generateRiver(
    Field& heightField,
    Field& riverField
) const
{
    for (int r = 0; r < settings_.numRivers; r++)
    {
        int width = heightField.width;
        int depth = heightField.depth;

        int x = rand() % width;
        int z = rand() % depth;

        for (int i = 0; i < 600; i++)
        {
            for (int dz = -2; dz <= 2; dz++)
            {
                for (int dx = -2; dx <= 2; dx++)
                {
                    int nx = x + dx;
                    int nz = z + dz;

                    if (nx >= 0 && nz >= 0 && nx < width && nz < depth)
                    {
                        riverField.at(nx, nz) = 1.0f;
                    }
                }
            }

            int bestX = x;
            int bestZ = z;
            float bestH = heightField.at(x, z);

            for (int dz = -1; dz <= 1; dz++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx;
                    int nz = z + dz;

                    if (nx < 0 || nz < 0 || nx >= width || nz >= depth)
                        continue;

                    float h = heightField.at(nx, nz);

                    if (h < bestH)
                    {
                        bestH = h;
                        bestX = nx;
                        bestZ = nz;
                    }
                }
            }

            x = bestX;
            z = bestZ;
        }
    }
}

void TerrainGenerator::applyConstraintSystem(
    Field& heightField,
    Field& riverField
) const
{
    int width = heightField.width;
    int depth = heightField.depth;

    Context ctx;
    ctx.height = heightField;
    ctx.river  = riverField;

    ctx.delta = Field{width, depth, std::vector<float>(width * depth, 0.0f)};
    ctx.slope = Field{width, depth, std::vector<float>(width * depth, 0.0f)};
    ctx.waterDist = Field{width, depth, std::vector<float>(width * depth, 0.0f)};

    std::vector<ConstraintNode*> graph;

    graph.push_back(new SlopeNode());
    // graph.push_back(new DistanceToWaterNode());
    if (settings_.enableRiver)
    {
        graph.push_back(new RiverNode(settings_.riverWeight,
                                    settings_.riverDepth));
    }
    graph.push_back(new SettlementNode());
    graph.push_back(new SmoothNode());

    for (int iter = 0; iter < settings_.constraintIterations; iter++)
    {
        std::fill(ctx.delta.data.begin(), ctx.delta.data.end(), 0.0f);

        for (ConstraintNode* node : graph)
        {
            node->apply(ctx);
        }

        for (int z = 0; z < depth; z++)
        {
            for (int x = 0; x < width; x++)
            {
                float d = ctx.delta.at(x, z);
                d = std::max(-0.01f, std::min(0.01f, d));

                ctx.height.at(x, z) += d;
            }
        }
    }

    heightField = ctx.height;

    for (ConstraintNode* node : graph)
        delete node;
}

TerrainGenerator::TerrainGenerator(TerrainSettings settings)
    : settings_(settings), permutation_(512, 0)
{
    if (settings_.width < 2 || settings_.depth < 2)
    {
        throw std::invalid_argument("Terrain dimensions must be at least 2x2");
    }
    reseed(settings_.seed);
}

void TerrainGenerator::setSettings(const TerrainSettings& settings)
{
    if (settings.width < 2 || settings.depth < 2)
    {
        throw std::invalid_argument("Terrain dimensions must be at least 2x2");
    }
    settings_ = settings;
    reseed(settings_.seed);
}

const TerrainSettings& TerrainGenerator::settings() const
{
    return settings_;
}

void TerrainGenerator::reseed(uint32_t seed)
{
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);
    for (int i = 0; i < 512; ++i)
    {
        permutation_[i] = p[i & 255];
    }
}

float TerrainGenerator::simplexNoise2D(float x, float y) const
{
    static constexpr float grads[8][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}, {0.7071f, 0.7071f}, {-0.7071f, 0.7071f}, {0.7071f, -0.7071f}, {-0.7071f, -0.7071f}};

    const float s = (x + y) * kF2;
    const int i = fastFloor(x + s);
    const int j = fastFloor(y + s);

    const float t = static_cast<float>(i + j) * kG2;
    const float x0 = x - (static_cast<float>(i) - t);
    const float y0 = y - (static_cast<float>(j) - t);

    int i1 = 0;
    int j1 = 0;
    if (x0 > y0)
    {
        i1 = 1;
        j1 = 0;
    }
    else
    {
        i1 = 0;
        j1 = 1;
    }

    const float x1 = x0 - static_cast<float>(i1) + kG2;
    const float y1 = y0 - static_cast<float>(j1) + kG2;
    const float x2 = x0 - 1.0f + 2.0f * kG2;
    const float y2 = y0 - 1.0f + 2.0f * kG2;

    const int ii = i & 255;
    const int jj = j & 255;

    const int gi0 = permutation_[ii + permutation_[jj]] & 7;
    const int gi1 = permutation_[ii + i1 + permutation_[jj + j1]] & 7;
    const int gi2 = permutation_[ii + 1 + permutation_[jj + 1]] & 7;

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0.0f)
    {
        t0 *= t0;
        n0 = t0 * t0 * (grads[gi0][0] * x0 + grads[gi0][1] * y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f)
    {
        t1 *= t1;
        n1 = t1 * t1 * (grads[gi1][0] * x1 + grads[gi1][1] * y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f)
    {
        t2 *= t2;
        n2 = t2 * t2 * (grads[gi2][0] * x2 + grads[gi2][1] * y2);
    }

    return 70.0f * (n0 + n1 + n2);
}

float TerrainGenerator::fbm(float x, float y, int octaves, float lacunarity, float gain) const
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = settings_.noise.frequency;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < octaves; ++octave)
    {
        value += amplitude * simplexNoise2D(x * frequency, y * frequency);
        amplitudeSum += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    if (amplitudeSum <= 0.0f)
    {
        return 0.0f;
    }
    return value / amplitudeSum;
}

float TerrainGenerator::ridgedFbm(
    float x,
    float y,
    int octaves,
    float lacunarity,
    float gain,
    float sharpness) const
{
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = settings_.noise.frequency;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < octaves; ++octave)
    {
        const float n = simplexNoise2D(x * frequency, y * frequency);
        float ridge = 1.0f - std::fabs(n);
        ridge = std::pow(clamp01(ridge), sharpness);

        value += ridge * amplitude;
        amplitudeSum += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    if (amplitudeSum <= 0.0f)
    {
        return 0.0f;
    }
    return value / amplitudeSum;
}

TerrainMesh TerrainGenerator::generateMesh() const
{
    TerrainMesh mesh;
    mesh.width = settings_.width;
    mesh.depth = settings_.depth;
    mesh.horizontalScale = settings_.horizontalScale;
    mesh.heights.resize(static_cast<size_t>(settings_.width) * static_cast<size_t>(settings_.depth), 0.0f);

    mesh.minHeight = std::numeric_limits<float>::max();
    mesh.maxHeight = std::numeric_limits<float>::lowest();

    const float centerX = static_cast<float>(settings_.width - 1) * 0.5f;
    const float centerZ = static_cast<float>(settings_.depth - 1) * 0.5f;
    const float maxRadius = std::min(centerX, centerZ) * settings_.horizontalScale;

    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(settings_.width) +
                               static_cast<size_t>(x);

            const float wx = static_cast<float>(x) * settings_.horizontalScale;
            const float wz = static_cast<float>(z) * settings_.horizontalScale;

            const float warpX = fbm(wx + 31.7f, wz - 18.2f, 3, settings_.noise.lacunarity, 0.5f) *
                                settings_.noise.warpAmplitude;
            const float warpZ = fbm(wx - 47.1f, wz + 22.8f, 3, settings_.noise.lacunarity, 0.5f) *
                                settings_.noise.warpAmplitude;

            const float sampleX = wx + warpX;
            const float sampleZ = wz + warpZ;

            const float continental = 0.5f * (fbm(sampleX, sampleZ, settings_.noise.octaves,
                                                  settings_.noise.lacunarity, settings_.noise.gain) +
                                              1.0f);
            const float ridges = ridgedFbm(sampleX + 101.3f, sampleZ - 77.9f, settings_.noise.octaves,
                                           settings_.noise.lacunarity, settings_.noise.gain,
                                           settings_.noise.ridgeSharpness);
            const float detail = 0.5f * (fbm(sampleX * 2.7f, sampleZ * 2.7f, 4, 2.0f, 0.5f) + 1.0f);

            float shape = 0.55f * continental + 0.35f * ridges + 0.10f * detail;

            if (settings_.islandFalloff)
            {
                const float dx = wx - centerX * settings_.horizontalScale;
                const float dz = wz - centerZ * settings_.horizontalScale;
                const float radius = std::sqrt(dx * dx + dz * dz);
                float t = 1.0f - radius / (maxRadius * settings_.falloffRadius);
                t = clamp01(t);
                shape *= std::pow(t, settings_.falloffPower);
            }

            const float height = shape * settings_.verticalScale;
            mesh.heights[idx] = height;
            mesh.minHeight = std::min(mesh.minHeight, height);
            mesh.maxHeight = std::max(mesh.maxHeight, height);
        }
    }

    Field heightField{
    settings_.width,
    settings_.depth,
    mesh.heights
    };

    Field riverField{
        settings_.width,
        settings_.depth,
        std::vector<float>(settings_.width * settings_.depth, 0.0f)
    };

    if (settings_.useConstraints)
    {
        generateRiver(heightField, riverField);
        applyConstraintSystem(heightField, riverField);

        mesh.heights = heightField.data;
        mesh.riverMask = riverField.data;

        mesh.minHeight = std::numeric_limits<float>::max();
        mesh.maxHeight = std::numeric_limits<float>::lowest();

        for (float h : mesh.heights)
        {
            mesh.minHeight = std::min(mesh.minHeight, h);
            mesh.maxHeight = std::max(mesh.maxHeight, h);
        }
    }

    mesh.vertices.resize(static_cast<size_t>(settings_.width) * static_cast<size_t>(settings_.depth));

    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(settings_.width) +
                               static_cast<size_t>(x);

            TerrainVertex v;
            v.x = static_cast<float>(x) * settings_.horizontalScale;
            v.y = mesh.heights[idx];
            v.z = static_cast<float>(z) * settings_.horizontalScale;
            mesh.vertices[idx] = v;
        }
    }

    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const int xL = std::max(0, x - 1);
            const int xR = std::min(settings_.width - 1, x + 1);
            const int zD = std::max(0, z - 1);
            const int zU = std::min(settings_.depth - 1, z + 1);

            const float hL = mesh.heights[static_cast<size_t>(z) * static_cast<size_t>(settings_.width) +
                                          static_cast<size_t>(xL)];
            const float hR = mesh.heights[static_cast<size_t>(z) * static_cast<size_t>(settings_.width) +
                                          static_cast<size_t>(xR)];
            const float hD = mesh.heights[static_cast<size_t>(zD) * static_cast<size_t>(settings_.width) +
                                          static_cast<size_t>(x)];
            const float hU = mesh.heights[static_cast<size_t>(zU) * static_cast<size_t>(settings_.width) +
                                          static_cast<size_t>(x)];

            const float dx = (hR - hL) / (static_cast<float>(xR - xL) * settings_.horizontalScale);
            const float dz = (hU - hD) / (static_cast<float>(zU - zD) * settings_.horizontalScale);

            const size_t idx = static_cast<size_t>(z) * static_cast<size_t>(settings_.width) +
                               static_cast<size_t>(x);
            TerrainVertex& v = mesh.vertices[idx];
            const float nx = -dx;
            const float ny = 1.0f;
            const float nz = -dz;
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            v.nx = nx * invLen;
            v.ny = ny * invLen;
            v.nz = nz * invLen;
        }
    }

    mesh.indices.reserve(static_cast<size_t>(settings_.width - 1) *
                         static_cast<size_t>(settings_.depth - 1) * 6);

    for (int z = 0; z < settings_.depth - 1; ++z)
    {
        for (int x = 0; x < settings_.width - 1; ++x)
        {
            const uint32_t i00 = static_cast<uint32_t>(z * settings_.width + x);
            const uint32_t i10 = static_cast<uint32_t>(z * settings_.width + (x + 1));
            const uint32_t i01 = static_cast<uint32_t>((z + 1) * settings_.width + x);
            const uint32_t i11 = static_cast<uint32_t>((z + 1) * settings_.width + (x + 1));

            mesh.indices.push_back(i00);
            mesh.indices.push_back(i10);
            mesh.indices.push_back(i01);

            mesh.indices.push_back(i10);
            mesh.indices.push_back(i11);
            mesh.indices.push_back(i01);
        }
    }

    return mesh;
}

} // namespace terrain
