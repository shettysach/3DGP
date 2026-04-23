#include "terrain.h"
#include "terrain/biomes.h"
#include "terrain/blending.h"
#include "terrain/climate.h"
#include "terrain/fields.h"
#include "terrain/landforms.h"
#include "terrain/mountains.h"
#include "terrain/plains.h"
#include "terrain/rivers.h"
#include "terrain/terrain_noise.h"
#include "terrain/util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

namespace terrain {

namespace {

constexpr float kSqrt3 = 1.7320508075688772f;
constexpr float kF2 = 0.5f * (kSqrt3 - 1.0f);
constexpr float kG2 = (3.0f - kSqrt3) / 6.0f;

int fastFloor(float x) {
    int x_int = static_cast<int>(x);
    return (x >= 0.0f) ? x_int : x_int - 1;
}

struct HeightGradient {
    float dx = 0.0f;
    float dz = 0.0f;
    float slope = 0.0f;
};

struct HeightStageInput {
    const TerrainSettings& settings;
    std::function<float(float, float, int, float, float)> fbm;
    std::function<float(float, float, int, float, float, float)> ridgedFbm;
};

float computeIslandFalloff(
    const TerrainSettings& settings,
    float worldX,
    float worldZ,
    float centerX,
    float centerZ,
    float maxRadius) {
    if (!settings.islandFalloff) {
        return 1.0f;
    }

    const float dx = worldX - centerX;
    const float dz = worldZ - centerZ;
    const float radius = std::sqrt(dx * dx + dz * dz);
    const float scaledRadius = std::max(0.0001f, maxRadius * settings.falloffRadius);
    float t = 1.0f - radius / scaledRadius;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::pow(t, settings.falloffPower);
}

TerrainFields buildBaseTerrainFields(const HeightStageInput& in) {
    TerrainFields fields(in.settings.width, in.settings.depth);
    const float centerX = static_cast<float>(in.settings.width - 1) * 0.5f * in.settings.horizontalScale;
    const float centerZ = static_cast<float>(in.settings.depth - 1) * 0.5f * in.settings.horizontalScale;
    const float maxRadius =
        std::min(static_cast<float>(in.settings.width - 1), static_cast<float>(in.settings.depth - 1)) *
        0.5f * in.settings.horizontalScale;
    const float baseFrequency = std::max(0.00001f, in.settings.noise.frequency);
    const float warpScale = in.settings.noise.warpFrequency / baseFrequency;

    for (int z = 0; z < in.settings.depth; ++z) {
        for (int x = 0; x < in.settings.width; ++x) {
            const size_t idx = fieldIndex(x, z, in.settings.width);
            const float worldX = static_cast<float>(x) * in.settings.horizontalScale;
            const float worldZ = static_cast<float>(z) * in.settings.horizontalScale;

            const float warpX = in.fbm(worldX * warpScale + 31.7f,
                                       worldZ * warpScale - 18.2f,
                                       3,
                                       in.settings.noise.lacunarity,
                                       0.5f) *
                                in.settings.noise.warpAmplitude;
            const float warpZ = in.fbm(worldX * warpScale - 47.1f,
                                       worldZ * warpScale + 22.8f,
                                       3,
                                       in.settings.noise.lacunarity,
                                       0.5f) *
                                in.settings.noise.warpAmplitude;

            const float sampleX = worldX + warpX;
            const float sampleZ = worldZ + warpZ;
            fields.sampleXs[idx] = sampleX;
            fields.sampleZs[idx] = sampleZ;

            const TerrainNoiseInput terrainNoiseIn{
                sampleX,
                sampleZ,
                in.settings.noise.octaves,
                in.settings.noise.lacunarity,
                in.settings.noise.gain,
                in.settings.noise.ridgeSharpness,
                in.fbm,
                in.ridgedFbm};
            const TerrainNoiseComputation terrainNoise = computeTerrainNoiseComputation(terrainNoiseIn);

            const MountainNoiseInput mountainNoiseIn{
                sampleX,
                sampleZ,
                in.settings.noise.octaves,
                in.settings.noise.lacunarity,
                in.settings.noise.gain,
                in.settings.noise.ridgeSharpness,
                in.settings.verticalScale,
                in.fbm,
                in.ridgedFbm};
            const MountainResult mountain = computeMountainResult(mountainNoiseIn, terrainNoise.detail);

            const PlainsNoiseInput plainsNoiseIn{
                sampleX,
                sampleZ,
                in.settings.noise.octaves,
                in.settings.noise.lacunarity,
                in.settings.noise.gain,
                in.settings.verticalScale,
                in.fbm};
            const float plainsHeight = computePlainsHeightFromNoise(plainsNoiseIn, terrainNoise.detail);

            const float falloff =
                computeIslandFalloff(in.settings, worldX, worldZ, centerX, centerZ, maxRadius);
            const BlendResult blend = blendTerrain(
                {mountain.height, mountain.weight, plainsHeight, terrainNoise.detail, falloff, in.settings.verticalScale});

            fields.heights[idx] = blend.height;
            fields.mountainWeights[idx] = blend.mountainWeight;
        }
    }

    return fields;
}

void applyRiverPass(TerrainFields& fields, const TerrainSettings& settings) {
    const RiverPassResult riverPass = runRiverPass(
        fields.heights,
        settings.width,
        settings.depth,
        settings.verticalScale,
        settings.rivers,
        settings.seed);
    fields.heights = riverPass.carvedHeights;
    fields.riverWeights = riverPass.riverWeights;
}

HeightGradient sampleHeightGradient(
    const std::vector<float>& heights,
    int width,
    int depth,
    float horizontalScale,
    int x,
    int z) {
    const int xL = std::max(0, x - 1);
    const int xR = std::min(width - 1, x + 1);
    const int zD = std::max(0, z - 1);
    const int zU = std::min(depth - 1, z + 1);

    const float hL = heights[fieldIndex(xL, z, width)];
    const float hR = heights[fieldIndex(xR, z, width)];
    const float hD = heights[fieldIndex(x, zD, width)];
    const float hU = heights[fieldIndex(x, zU, width)];

    const float dx = (hR - hL) / (static_cast<float>(xR - xL) * horizontalScale);
    const float dz = (hU - hD) / (static_cast<float>(zU - zD) * horizontalScale);
    return {dx, dz, std::sqrt(dx * dx + dz * dz)};
}

void computeSlopeField(TerrainFields& fields, float horizontalScale) {
    for (int z = 0; z < fields.depth; ++z) {
        for (int x = 0; x < fields.width; ++x) {
            const size_t idx = fieldIndex(x, z, fields.width);
            fields.slopes[idx] =
                sampleHeightGradient(fields.heights, fields.width, fields.depth, horizontalScale, x, z).slope;
        }
    }
}

void buildVertices(
    TerrainMesh& mesh,
    const TerrainFields& fields,
    const TerrainSettings& settings) {
    mesh.vertices.resize(static_cast<size_t>(settings.width) * static_cast<size_t>(settings.depth));

    for (int z = 0; z < settings.depth; ++z) {
        for (int x = 0; x < settings.width; ++x) {
            const size_t idx = fieldIndex(x, z, settings.width);

            TerrainVertex v;
            v.x = static_cast<float>(x) * settings.horizontalScale;
            v.y = mesh.heights[idx];
            v.z = static_cast<float>(z) * settings.horizontalScale;
            v.slope = fields.slopes[idx];
            v.mountainWeight = fields.mountainWeights[idx];
            v.plainsWeight = 1.0f - fields.mountainWeights[idx];
            v.riverWeight = fields.riverWeights[idx];
            v.temperature = fields.temperature[idx];
            v.precipitation = fields.precipitation[idx];
            v.moisture = fields.moisture[idx];
            v.landform = fields.landformIds[idx];
            v.ecology = fields.ecologyIds[idx];
            v.primaryBiome = fields.primaryBiomeIds[idx];
            v.secondaryBiome = fields.secondaryBiomeIds[idx];
            v.primaryBiomeWeight = fields.primaryBiomeWeights[idx];
            v.secondaryBiomeWeight = fields.secondaryBiomeWeights[idx];
            mesh.vertices[idx] = v;
        }
    }

    for (int z = 0; z < settings.depth; ++z) {
        for (int x = 0; x < settings.width; ++x) {
            const HeightGradient gradient =
                sampleHeightGradient(mesh.heights, settings.width, settings.depth, settings.horizontalScale, x, z);
            const size_t idx = fieldIndex(x, z, settings.width);
            TerrainVertex& v = mesh.vertices[idx];
            const float nx = -gradient.dx;
            const float ny = 1.0f;
            const float nz = -gradient.dz;
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            v.nx = nx * invLen;
            v.ny = ny * invLen;
            v.nz = nz * invLen;
        }
    }
}

void buildGridIndices(TerrainMesh& mesh, int width, int depth) {
    mesh.indices.reserve(static_cast<size_t>(width - 1) *
                         static_cast<size_t>(depth - 1) * 6);

    for (int z = 0; z < depth - 1; ++z) {
        for (int x = 0; x < width - 1; ++x) {
            const uint32_t i00 = static_cast<uint32_t>(z * width + x);
            const uint32_t i10 = static_cast<uint32_t>(z * width + (x + 1));
            const uint32_t i01 = static_cast<uint32_t>((z + 1) * width + x);
            const uint32_t i11 = static_cast<uint32_t>((z + 1) * width + (x + 1));

            mesh.indices.push_back(i00);
            mesh.indices.push_back(i10);
            mesh.indices.push_back(i01);

            mesh.indices.push_back(i10);
            mesh.indices.push_back(i11);
            mesh.indices.push_back(i01);
        }
    }
}

} // namespace

TerrainGenerator::TerrainGenerator(TerrainSettings settings)
    : settings_(settings), permutation_(512, 0) {
    if (settings_.width < 2 || settings_.depth < 2) {
        throw std::invalid_argument("Terrain dimensions must be at least 2x2");
    }
    reseed(settings_.seed);
}

void TerrainGenerator::setSettings(const TerrainSettings& settings) {
    if (settings.width < 2 || settings.depth < 2) {
        throw std::invalid_argument("Terrain dimensions must be at least 2x2");
    }
    settings_ = settings;
    reseed(settings_.seed);
}

const TerrainSettings& TerrainGenerator::settings() const {
    return settings_;
}

void TerrainGenerator::reseed(uint32_t seed) {
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);
    for (int i = 0; i < 512; ++i) {
        permutation_[i] = p[i & 255];
    }
}

float TerrainGenerator::simplexNoise2D(float x, float y) const {
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
    if (x0 > y0) {
        i1 = 1;
        j1 = 0;
    } else {
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
    if (t0 > 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * (grads[gi0][0] * x0 + grads[gi0][1] * y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * (grads[gi1][0] * x1 + grads[gi1][1] * y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * (grads[gi2][0] * x2 + grads[gi2][1] * y2);
    }

    return 70.0f * (n0 + n1 + n2);
}

// Layers multiple noise, adds detail
float TerrainGenerator::fractalBrownianMotion(float x, float y, int octaves, float lacunarity, float gain) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, [](float n) { return n; });
}

// Creates mountain ridges
float TerrainGenerator::ridgedFbm(
    float x,
    float y,
    int octaves,
    float lacunarity,
    float gain,
    float sharpness) const {
    return octaveNoise(x, y, octaves, lacunarity, gain, [sharpness](float n) { return std::pow(std::clamp(1.0f - std::fabs(n), 0.0f, 1.0f), sharpness); });
}

// Main generation function
// 1. Builds the scalar terrain fields (height, mountain mask, warped sample positions)
// 2. Applies post-processing stages (smoothing, slopes, climate, landforms, biomes)
// 3. Packs those fields into the renderable mesh
TerrainMesh TerrainGenerator::generateMesh() const {
    using Clock = std::chrono::steady_clock;
    const auto totalStart = Clock::now();
    auto stageStart = totalStart;
    const auto stageMs = [](const Clock::time_point& start, const Clock::time_point& end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };

    const auto fbm = [this](float x, float y, int octaves, float lacunarity, float gain) {
        return this->fractalBrownianMotion(x, y, octaves, lacunarity, gain);
    };
    const auto ridged = [this](float x, float y, int octaves, float lacunarity, float gain, float sharpness) {
        return this->ridgedFbm(x, y, octaves, lacunarity, gain, sharpness);
    };

    TerrainFields fields = buildBaseTerrainFields({settings_, fbm, ridged});
    const auto baseTerrainDone = Clock::now();
    smoothHeights(fields.heights, fields.mountainWeights, settings_.width, settings_.depth);
    const auto smoothDone = Clock::now();
    applyRiverPass(fields, settings_);
    const auto riversDone = Clock::now();
    computeSlopeField(fields, settings_.horizontalScale);
    const auto slopesDone = Clock::now();
    computeClimateFields(fields, settings_, fbm);
    const auto climateDone = Clock::now();
    computeLandformFields(fields);
    const auto landformsDone = Clock::now();
    computeBiomeFields(fields);
    const auto biomesDone = Clock::now();

    TerrainMesh mesh;
    mesh.width = settings_.width;
    mesh.depth = settings_.depth;
    mesh.horizontalScale = settings_.horizontalScale;
    mesh.heights = fields.heights;
    mesh.temperatureMap = fields.temperature;
    mesh.precipitationMap = fields.precipitation;
    mesh.moistureMap = fields.moisture;
    computeHeightExtents(mesh.heights, mesh.minHeight, mesh.maxHeight);
    const auto extentsDone = Clock::now();

    buildVertices(mesh, fields, settings_);
    const auto verticesDone = Clock::now();
    buildGridIndices(mesh, settings_.width, settings_.depth);
    const auto indicesDone = Clock::now();

    std::cout << "[profile] terrain " << settings_.width << 'x' << settings_.depth
              << " base=" << stageMs(stageStart, baseTerrainDone) << "ms"
              << " smooth=" << stageMs(baseTerrainDone, smoothDone) << "ms"
              << " rivers=" << stageMs(smoothDone, riversDone) << "ms"
              << " slopes=" << stageMs(riversDone, slopesDone) << "ms"
              << " climate=" << stageMs(slopesDone, climateDone) << "ms"
              << " landforms=" << stageMs(climateDone, landformsDone) << "ms"
              << " biomes=" << stageMs(landformsDone, biomesDone) << "ms"
              << " extents=" << stageMs(biomesDone, extentsDone) << "ms"
              << " vertices=" << stageMs(extentsDone, verticesDone) << "ms"
              << " indices=" << stageMs(verticesDone, indicesDone) << "ms"
              << " total=" << stageMs(totalStart, indicesDone) << "ms"
              << " verts=" << mesh.vertices.size()
              << " tris=" << (mesh.indices.size() / 3u) << '\n';

    return mesh;
}

} // namespace terrain
