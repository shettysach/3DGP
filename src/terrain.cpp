#include "terrain.h"
#include "terrain/biomes.h"
#include "terrain/blending.h"
#include "terrain/fields.h"
#include "terrain/landforms.h"
#include "terrain/mountains.h"
#include "terrain/plains.h"
#include "terrain/plateaus.h"
#include "terrain/rivers.h"
#include "terrain/util.h"
#include "terrain/valleys.h"

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
            const HeightGradient gradient =
                sampleHeightGradient(fields.heights, fields.width, fields.depth, horizontalScale, x, z);
            fields.gradientXs[idx] = gradient.dx;
            fields.gradientZs[idx] = gradient.dz;
            fields.slopes[idx] = gradient.slope;
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
            v.y = fields.heights[idx];
            v.z = static_cast<float>(z) * settings.horizontalScale;
            v.slope = fields.slopes[idx];
            v.mountainWeight = fields.mountainWeights[idx];
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
            const float nx = -fields.gradientXs[idx];
            const float ny = 1.0f;
            const float nz = -fields.gradientZs[idx];
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            v.nx = nx * invLen;
            v.ny = ny * invLen;
            v.nz = nz * invLen;
            mesh.vertices[idx] = v;
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

TerrainFields TerrainGenerator::buildBaseTerrainFields() const {
    TerrainFields fields(settings_.width, settings_.depth);
    const float centerX = static_cast<float>(settings_.width - 1) * 0.5f * settings_.horizontalScale;
    const float centerZ = static_cast<float>(settings_.depth - 1) * 0.5f * settings_.horizontalScale;
    const float maxRadius =
        std::min(static_cast<float>(settings_.width - 1), static_cast<float>(settings_.depth - 1)) *
        0.5f * settings_.horizontalScale;
    const float baseFrequency = std::max(0.00001f, settings_.noise.frequency);
    const float warpScale = settings_.noise.warpFrequency / baseFrequency;

    for (int z = 0; z < settings_.depth; ++z) {
        for (int x = 0; x < settings_.width; ++x) {
            const size_t idx = fieldIndex(x, z, settings_.width);
            const float worldX = static_cast<float>(x) * settings_.horizontalScale;
            const float worldZ = static_cast<float>(z) * settings_.horizontalScale;

            const float warpX = fractalBrownianMotion(worldX * warpScale + 31.7f,
                                                      worldZ * warpScale - 18.2f,
                                                      3,
                                                      settings_.noise.lacunarity,
                                                      0.5f) *
                                settings_.noise.warpAmplitude;
            const float warpZ = fractalBrownianMotion(worldX * warpScale - 47.1f,
                                                      worldZ * warpScale + 22.8f,
                                                      3,
                                                      settings_.noise.lacunarity,
                                                      0.5f) *
                                settings_.noise.warpAmplitude;

            const float sampleX = worldX + warpX;
            const float sampleZ = worldZ + warpZ;
            fields.sampleXs[idx] = sampleX;
            fields.sampleZs[idx] = sampleZ;

            const float continental =
                0.5f * (fractalBrownianMotion(sampleX, sampleZ, settings_.noise.octaves, settings_.noise.lacunarity, settings_.noise.gain) + 1.0f);
            const float detail =
                0.5f * (fractalBrownianMotion(sampleX * 2.7f, sampleZ * 2.7f, 4, 2.0f, 0.5f) + 1.0f);

            const float ridges = ridgedFbm(
                sampleX + 101.3f,
                sampleZ - 77.9f,
                settings_.noise.octaves,
                settings_.noise.lacunarity,
                settings_.noise.gain,
                settings_.noise.ridgeSharpness);
            const float rangeMask = smoothstep(
                0.42f,
                0.72f,
                0.5f * (fractalBrownianMotion(
                            sampleX * 0.3f + 400.0f,
                            sampleZ * 0.3f - 250.0f,
                            3,
                            settings_.noise.lacunarity,
                            0.45f) +
                        1.0f));
            const float slopeHint = std::clamp((ridges - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);
            const MountainResult mountain = computeMountain(
                {continental, ridges, detail, slopeHint, rangeMask, settings_.verticalScale});

            const float basin = 0.5f * (fractalBrownianMotion(
                                            sampleX * 0.28f - 191.7f,
                                            sampleZ * 0.28f + 83.4f,
                                            3,
                                            settings_.noise.lacunarity,
                                            0.52f) +
                                        1.0f);
            const float detailBand = 0.5f * (fractalBrownianMotion(
                                                 sampleX * 1.9f + 52.3f,
                                                 sampleZ * 1.9f - 61.8f,
                                                 std::max(3, settings_.noise.octaves - 2),
                                                 settings_.noise.lacunarity,
                                                 settings_.noise.gain) +
                                             1.0f);
            const float rimMask = smoothstep(
                0.38f,
                0.74f,
                0.5f * (fractalBrownianMotion(
                            sampleX * 0.17f + 420.0f,
                            sampleZ * 0.17f - 301.0f,
                            3,
                            settings_.noise.lacunarity,
                            0.45f) +
                        1.0f));
            const float valleySlopeHint = std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);
            const ValleyResult valley = computeValley(
                {continental, basin, detailBand, valleySlopeHint, rimMask, settings_.verticalScale});

            const int plainsOctaves = std::max(3, settings_.noise.octaves - 2);
            const float plainsBase = 0.5f * (fractalBrownianMotion(
                                                 sampleX - 63.2f,
                                                 sampleZ + 41.8f,
                                                 plainsOctaves,
                                                 settings_.noise.lacunarity,
                                                 settings_.noise.gain) +
                                             1.0f);
            const float macroRelief = 0.5f * (fractalBrownianMotion(
                                                  sampleX * 0.30f + 219.4f,
                                                  sampleZ * 0.30f - 174.6f,
                                                  3,
                                                  settings_.noise.lacunarity,
                                                  0.48f) +
                                              1.0f);
            const float hilliness = 0.5f * (fractalBrownianMotion(
                                                sampleX * 0.82f - 141.5f,
                                                sampleZ * 0.82f + 96.8f,
                                                std::max(3, settings_.noise.octaves - 1),
                                                settings_.noise.lacunarity,
                                                settings_.noise.gain) +
                                            1.0f);
            const float basinNoise = 0.5f * (fractalBrownianMotion(
                                                 sampleX * 0.18f - 331.7f,
                                                 sampleZ * 0.18f + 271.4f,
                                                 2,
                                                 settings_.noise.lacunarity,
                                                 0.55f) +
                                             1.0f);
            const float plainsHeight = computePlainsHeight(
                {continental, plainsBase, macroRelief, hilliness, basinNoise, detail, settings_.verticalScale});

            const float plateauMask = 0.5f * (fractalBrownianMotion(
                                                  sampleX * settings_.plateaus.frequency,
                                                  sampleZ * settings_.plateaus.frequency,
                                                  3,
                                                  settings_.noise.lacunarity,
                                                  0.52f) +
                                              1.0f);
            const PlateauResult plateau = computePlateau(
                {continental, plateauMask, detail, settings_.verticalScale});

            const float falloff =
                computeIslandFalloff(settings_, worldX, worldZ, centerX, centerZ, maxRadius);
            const BlendResult blend = blendTerrain(
                {mountain.height, mountain.weight, plainsHeight, plateau.height, plateau.weight, valley.depth, detail, falloff, settings_.verticalScale});

            fields.heights[idx] = blend.height;
            fields.mountainWeights[idx] = blend.mountainWeight;
            fields.valleyWeights[idx] = valley.weight;
            fields.plateauWeights[idx] = plateau.weight;
        }
    }

    return fields;
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

    TerrainFields fields = buildBaseTerrainFields();
    const auto baseTerrainDone = Clock::now();
    smoothHeights(fields.heights, fields.mountainWeights, fields.valleyWeights, settings_.width, settings_.depth);
    const auto smoothDone = Clock::now();
    applyRiverPass(fields, settings_);
    const auto riversDone = Clock::now();
    computeHeightExtents(fields.heights, fields.minHeight, fields.maxHeight);
    computeSlopeField(fields, settings_.horizontalScale);
    const auto slopesDone = Clock::now();
    computeClimateFields(fields);
    const auto climateDone = Clock::now();
    computeLandformFields(fields);
    const auto landformsDone = Clock::now();
    computeBiomeFields(fields);
    const auto biomesDone = Clock::now();

    TerrainMesh mesh;
    mesh.width = settings_.width;
    mesh.depth = settings_.depth;
    mesh.horizontalScale = settings_.horizontalScale;
    mesh.minHeight = fields.minHeight;
    mesh.maxHeight = fields.maxHeight;
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
