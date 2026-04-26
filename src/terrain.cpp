#include "terrain.h"
#include "graph/graph_execute.h"
#include "graph/types.h"
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
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

namespace terrain {

namespace {

enum class MacroRegionType : uint8_t {
    Neutral = 0,
    Mountain = 1,
    Dry = 2,
    Wet = 3,
    Coastal = 4,
};

struct VoronoiSeed {
    float x = 0.0f;
    float z = 0.0f;
    MacroRegionType type = MacroRegionType::Neutral;
};

struct VoronoiGrid {
    int cellCountX = 0;
    int cellCountZ = 0;
    int minCellX = 0;
    int minCellZ = 0;
    int spanX = 0;
    int spanZ = 0;
    float cellWorldSizeX = 1.0f;
    float cellWorldSizeZ = 1.0f;
    std::vector<VoronoiSeed> seeds;
};

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

VoronoiGrid buildVoronoiGrid(const TerrainSettings& settings, uint32_t seed) {
    const int area = settings.width * settings.depth;
    const int desiredCells = std::clamp(area / (140 * 140), 24, 96);
    const float aspect = static_cast<float>(settings.width) / std::max(1.0f, static_cast<float>(settings.depth));
    const int cellCountX = std::max(4, static_cast<int>(std::round(std::sqrt(desiredCells * aspect))));
    const int cellCountZ = std::max(4, static_cast<int>(std::ceil(static_cast<float>(desiredCells) / cellCountX)));
    const float mapMaxX = static_cast<float>(settings.width - 1) * settings.horizontalScale;
    const float mapMaxZ = static_cast<float>(settings.depth - 1) * settings.horizontalScale;
    const float borderNormScale = std::max(0.001f, std::min(mapMaxX, mapMaxZ));

    VoronoiGrid grid;
    grid.cellCountX = cellCountX;
    grid.cellCountZ = cellCountZ;
    grid.minCellX = -1;
    grid.minCellZ = -1;
    grid.spanX = cellCountX + 2;
    grid.spanZ = cellCountZ + 2;
    grid.cellWorldSizeX = std::max(1.0f, mapMaxX / static_cast<float>(cellCountX));
    grid.cellWorldSizeZ = std::max(1.0f, mapMaxZ / static_cast<float>(cellCountZ));
    grid.seeds.resize(static_cast<size_t>(grid.spanX) * static_cast<size_t>(grid.spanZ));

    for (int cellZ = grid.minCellZ; cellZ <= cellCountZ; ++cellZ) {
        for (int cellX = grid.minCellX; cellX <= cellCountX; ++cellX) {
            const size_t cellHash = (static_cast<size_t>(cellX - grid.minCellX) * 92821u) +
                                    (static_cast<size_t>(cellZ - grid.minCellZ) * 68917u);
            const float jx = hashJitter(cellHash * 9781u + 17u, seed);
            const float jz = hashJitter(cellHash * 3253u + 59u, seed + 11u);
            const float x = (static_cast<float>(cellX) + 0.1f + 0.8f * jx) * grid.cellWorldSizeX;
            const float z = (static_cast<float>(cellZ) + 0.1f + 0.8f * jz) * grid.cellWorldSizeZ;
            const float distToBorder = std::min(std::min(x, mapMaxX - x), std::min(z, mapMaxZ - z));
            const float borderNorm = std::clamp(distToBorder / borderNormScale, 0.0f, 1.0f);

            MacroRegionType type = MacroRegionType::Neutral;
            if (borderNorm < 0.10f) {
                type = MacroRegionType::Coastal;
            } else {
                const float roll = hashJitter(cellHash * 6151u + 211u, seed + 23u);
                if (roll < 0.27f) {
                    type = MacroRegionType::Mountain;
                } else if (roll < 0.50f) {
                    type = MacroRegionType::Dry;
                } else if (roll < 0.73f) {
                    type = MacroRegionType::Wet;
                }
            }

            const int localX = cellX - grid.minCellX;
            const int localZ = cellZ - grid.minCellZ;
            const size_t idx = static_cast<size_t>(localZ) * static_cast<size_t>(grid.spanX) +
                               static_cast<size_t>(localX);
            grid.seeds[idx] = {x, z, type};
        }
    }

    return grid;
}

const VoronoiSeed& seedAt(const VoronoiGrid& grid, int cellX, int cellZ) {
    const int clampedCellX = std::clamp(cellX, grid.minCellX, grid.minCellX + grid.spanX - 1);
    const int clampedCellZ = std::clamp(cellZ, grid.minCellZ, grid.minCellZ + grid.spanZ - 1);
    const int localX = clampedCellX - grid.minCellX;
    const int localZ = clampedCellZ - grid.minCellZ;
    const size_t idx = static_cast<size_t>(localZ) * static_cast<size_t>(grid.spanX) +
                       static_cast<size_t>(localX);
    return grid.seeds[idx];
}

void applyVoronoiMacroRegions(
    TerrainFields& fields,
    const TerrainSettings& settings,
    const NoiseContext& noise,
    uint32_t seed) {
    const VoronoiGrid grid = buildVoronoiGrid(settings, seed);
    if (grid.seeds.empty()) {
        return;
    }

    const float warpFrequency = 0.0018f;
    const float warpAmplitude = 0.42f * std::min(grid.cellWorldSizeX, grid.cellWorldSizeZ);
    const float edgeBlendWorld = 0.55f * std::min(grid.cellWorldSizeX, grid.cellWorldSizeZ);

    std::array<int, 5> regionCounts = {0, 0, 0, 0, 0};

    for (int z = 0; z < settings.depth; ++z) {
        for (int x = 0; x < settings.width; ++x) {
            const size_t idx = fieldIndex(x, z, settings.width);
            const float worldX = static_cast<float>(x) * settings.horizontalScale;
            const float worldZ = static_cast<float>(z) * settings.horizontalScale;
            const float offsetX = noise.simplex2D(worldX * warpFrequency + 31.7f, worldZ * warpFrequency + 19.3f) *
                                  warpAmplitude;
            const float offsetZ = noise.simplex2D(worldX * warpFrequency + 67.1f, worldZ * warpFrequency + 73.9f) *
                                  warpAmplitude;
            const float warpedX = worldX + offsetX;
            const float warpedZ = worldZ + offsetZ;
            const int centerCellX = static_cast<int>(std::floor(warpedX / grid.cellWorldSizeX));
            const int centerCellZ = static_cast<int>(std::floor(warpedZ / grid.cellWorldSizeZ));

            float nearestDistSq = std::numeric_limits<float>::max();
            float secondDistSq = std::numeric_limits<float>::max();
            const VoronoiSeed* nearestSeed = nullptr;

            // Video method: nearest cell is inside 4x4 nearby quadrants.
            for (int cellZ = centerCellZ - 1; cellZ <= centerCellZ + 2; ++cellZ) {
                for (int cellX = centerCellX - 1; cellX <= centerCellX + 2; ++cellX) {
                    const VoronoiSeed& seedInfo = seedAt(grid, cellX, cellZ);
                    const float dx = warpedX - seedInfo.x;
                    const float dz = warpedZ - seedInfo.z;
                    const float distSq = dx * dx + dz * dz;
                    if (distSq < nearestDistSq) {
                        secondDistSq = nearestDistSq;
                        nearestDistSq = distSq;
                        nearestSeed = &seedInfo;
                    } else if (distSq < secondDistSq) {
                        secondDistSq = distSq;
                    }
                }
            }

            if (nearestSeed == nullptr) {
                continue;
            }

            const float nearestDist = std::sqrt(std::max(0.0f, nearestDistSq));
            const float secondDist = std::sqrt(std::max(0.0f, secondDistSq));
            const float borderGap = std::max(0.0f, secondDist - nearestDist);
            const float influence = std::clamp(smoothstep(0.0f, edgeBlendWorld, borderGap), 0.0f, 1.0f);

            switch (nearestSeed->type) {
            case MacroRegionType::Mountain:
                fields.heights[idx] += settings.verticalScale * 0.052f * influence;
                fields.mountainWeights[idx] = std::clamp(fields.mountainWeights[idx] + 0.22f * influence, 0.0f, 1.0f);
                break;
            case MacroRegionType::Dry:
                fields.heights[idx] += settings.verticalScale * 0.012f * influence;
                break;
            case MacroRegionType::Wet:
                fields.heights[idx] -= settings.verticalScale * 0.010f * influence;
                fields.valleyWeights[idx] = std::clamp(fields.valleyWeights[idx] + 0.12f * influence, 0.0f, 1.0f);
                break;
            case MacroRegionType::Coastal:
                fields.heights[idx] -= settings.verticalScale * 0.018f * influence;
                break;
            case MacroRegionType::Neutral:
            default:
                break;
            }

            ++regionCounts[static_cast<size_t>(nearestSeed->type)];
        }
    }

    std::cout << "[voronoi] regions neutral=" << regionCounts[0]
              << " mountain=" << regionCounts[1]
              << " dry=" << regionCounts[2]
              << " wet=" << regionCounts[3]
              << " coastal=" << regionCounts[4]
              << " cells=" << grid.cellCountX << "x" << grid.cellCountZ
              << " warp=" << warpAmplitude << '\n';
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
    : settings_(settings) {
    noiseContext_.permutation.resize(512, 0);
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

void TerrainGenerator::setBaseGraph(std::shared_ptr<const graph::CompiledGraph> graph) {
    baseGraph_ = std::move(graph);
}

const graph::CompiledGraph* TerrainGenerator::baseGraph() const {
    return baseGraph_.get();
}

void TerrainGenerator::reseed(uint32_t seed) {
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);
    std::mt19937 rng(seed);
    std::shuffle(p.begin(), p.end(), rng);
    noiseContext_.permutation.resize(512);
    for (int i = 0; i < 512; ++i) {
        noiseContext_.permutation[i] = p[i & 255];
    }
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

            const float warpX = noiseContext_.fbm(worldX * warpScale + 31.7f,
                                                   worldZ * warpScale - 18.2f,
                                                   3,
                                                   settings_.noise.lacunarity,
                                                   0.5f,
                                                   baseFrequency) *
                                settings_.noise.warpAmplitude;
            const float warpZ = noiseContext_.fbm(worldX * warpScale - 47.1f,
                                                   worldZ * warpScale + 22.8f,
                                                   3,
                                                   settings_.noise.lacunarity,
                                                   0.5f,
                                                   baseFrequency) *
                                settings_.noise.warpAmplitude;

            const float sampleX = worldX + warpX;
            const float sampleZ = worldZ + warpZ;
            fields.sampleXs[idx] = sampleX;
            fields.sampleZs[idx] = sampleZ;

            const float continental =
                0.5f * (noiseContext_.fbm(sampleX, sampleZ, settings_.noise.octaves, settings_.noise.lacunarity, settings_.noise.gain, baseFrequency) + 1.0f);
            const float detail =
                0.5f * (noiseContext_.fbm(sampleX * 2.7f, sampleZ * 2.7f, 4, 2.0f, 0.5f, baseFrequency) + 1.0f);

            const float ridges = noiseContext_.ridgedFbm(
                sampleX + 101.3f,
                sampleZ - 77.9f,
                settings_.noise.octaves,
                settings_.noise.lacunarity,
                settings_.noise.gain,
                settings_.noise.ridgeSharpness,
                baseFrequency);
            const float rangeMask = smoothstep(
                0.42f,
                0.72f,
                0.5f * (noiseContext_.fbm(
                            sampleX * 0.3f + 400.0f,
                            sampleZ * 0.3f - 250.0f,
                            3,
                            settings_.noise.lacunarity,
                            0.45f,
                            baseFrequency) +
                        1.0f));
            const float slopeHint = std::clamp((ridges - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);
            const MountainResult mountain = computeMountain(
                {continental, ridges, detail, slopeHint, rangeMask, settings_.verticalScale});

            const float basin = 0.5f * (noiseContext_.fbm(
                                            sampleX * 0.28f - 191.7f,
                                            sampleZ * 0.28f + 83.4f,
                                            3,
                                            settings_.noise.lacunarity,
                                            0.52f,
                                            baseFrequency) +
                                        1.0f);
            const float detailBand = 0.5f * (noiseContext_.fbm(
                                                 sampleX * 1.9f + 52.3f,
                                                 sampleZ * 1.9f - 61.8f,
                                                 std::max(3, settings_.noise.octaves - 2),
                                                 settings_.noise.lacunarity,
                                                 settings_.noise.gain,
                                                 baseFrequency) +
                                              1.0f);
            const float rimMask = smoothstep(
                0.38f,
                0.74f,
                0.5f * (noiseContext_.fbm(
                            sampleX * 0.17f + 420.0f,
                            sampleZ * 0.17f - 301.0f,
                            3,
                            settings_.noise.lacunarity,
                            0.45f,
                            baseFrequency) +
                        1.0f));
            const float valleySlopeHint = std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);
            const ValleyResult valley = computeValley(
                {continental, basin, detailBand, valleySlopeHint, rimMask, settings_.verticalScale});

            const int plainsOctaves = std::max(3, settings_.noise.octaves - 2);
            const float plainsBase = 0.5f * (noiseContext_.fbm(
                                                 sampleX - 63.2f,
                                                 sampleZ + 41.8f,
                                                 plainsOctaves,
                                                 settings_.noise.lacunarity,
                                                 settings_.noise.gain,
                                                 baseFrequency) +
                                              1.0f);
            const float macroRelief = 0.5f * (noiseContext_.fbm(
                                                   sampleX * 0.30f + 219.4f,
                                                   sampleZ * 0.30f - 174.6f,
                                                   3,
                                                   settings_.noise.lacunarity,
                                                   0.48f,
                                                   baseFrequency) +
                                               1.0f);
            const float hilliness = 0.5f * (noiseContext_.fbm(
                                                 sampleX * 0.82f - 141.5f,
                                                 sampleZ * 0.82f + 96.8f,
                                                 std::max(3, settings_.noise.octaves - 1),
                                                 settings_.noise.lacunarity,
                                                 settings_.noise.gain,
                                                 baseFrequency) +
                                             1.0f);
            const float basinNoise = 0.5f * (noiseContext_.fbm(
                                                  sampleX * 0.18f - 331.7f,
                                                  sampleZ * 0.18f + 271.4f,
                                                  2,
                                                  settings_.noise.lacunarity,
                                                  0.55f,
                                                  baseFrequency) +
                                              1.0f);
            const float plainsHeight = computePlainsHeight(
                {continental, plainsBase, macroRelief, hilliness, basinNoise, detail, settings_.verticalScale});

            const float plateauMask = 0.5f * (noiseContext_.fbm(
                                                   sampleX * settings_.plateaus.frequency,
                                                   sampleZ * settings_.plateaus.frequency,
                                                   3,
                                                   settings_.noise.lacunarity,
                                                   0.52f,
                                                   baseFrequency) +
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

    TerrainFields fields =
        baseGraph_ ? graph::execute(*baseGraph_, settings_, noiseContext_)
                   : buildBaseTerrainFields();
    const auto baseTerrainDone = Clock::now();
    if (settings_.enableVoronoi) {
        applyVoronoiMacroRegions(fields, settings_, noiseContext_, settings_.seed);
    }
    const auto voronoiDone = Clock::now();
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
              << " voronoi=" << stageMs(baseTerrainDone, voronoiDone) << "ms"
              << " smooth=" << stageMs(voronoiDone, smoothDone) << "ms"
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
