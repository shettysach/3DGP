#include "terrain.h"
#include "graph/graph_compile.h"
#include "graph/graph_execute.h"
#include "graph/types.h"
#include "terrain/biomes.h"
#include "terrain/blending.h"
#include "terrain/fields.h"
#include "terrain/landforms.h"
#include "terrain/rivers.h"
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

struct HeightGradient {
    float dx = 0.0f;
    float dz = 0.0f;
    float slope = 0.0f;
};

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
    baseGraph_ = std::make_shared<graph::CompiledGraph>(graph::compile(graph::defaultGraph()));
}

void TerrainGenerator::setSettings(const TerrainSettings& settings) {
    if (settings.width < 2 || settings.depth < 2) {
        throw std::invalid_argument("Terrain dimensions must be at least 2x2");
    }
    settings_ = settings;
    reseed(settings_.seed);
}

void TerrainGenerator::setBaseGraph(std::shared_ptr<const graph::CompiledGraph> graph) {
    baseGraph_ = std::move(graph);
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

    if (!baseGraph_) {
        throw std::runtime_error("No compiled graph set on TerrainGenerator");
    }
    TerrainFields fields = graph::execute(*baseGraph_, settings_, noiseContext_);
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
