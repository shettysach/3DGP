#include "terrain.h"
#include "terrain/blending.h"
#include "terrain/biomes.h"
#include "terrain/climate.h"
#include "terrain/fields.h"
#include "terrain/landforms.h"
#include "terrain/mountains.h"
#include "terrain/plains.h"
#include "terrain/provinces.h"
#include "terrain/rivers.h"
#include "terrain/terrain_noise.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
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

float placementJitter(size_t idx, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(idx) ^ (seed * 747796405u + 2891336453u);
    h ^= (h >> 16);
    h *= 2246822519u;
    h ^= (h >> 13);
    h *= 3266489917u;
    h ^= (h >> 16);
    return static_cast<float>(h & 1023u) / 1023.0f;
}

struct HeightGradient
{
    float dx = 0.0f;
    float dz = 0.0f;
    float slope = 0.0f;
};

struct HeightStageInput
{
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
    float maxRadius)
{
    if (!settings.islandFalloff)
    {
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

TerrainFields buildBaseTerrainFields(const HeightStageInput& in)
{
    TerrainFields fields(in.settings.width, in.settings.depth);
    const float centerX = static_cast<float>(in.settings.width - 1) * 0.5f * in.settings.horizontalScale;
    const float centerZ = static_cast<float>(in.settings.depth - 1) * 0.5f * in.settings.horizontalScale;
    const float maxRadius =
        std::min(static_cast<float>(in.settings.width - 1), static_cast<float>(in.settings.depth - 1)) *
        0.5f * in.settings.horizontalScale;
    const float baseFrequency = std::max(0.00001f, in.settings.noise.frequency);
    const float warpScale = in.settings.noise.warpFrequency / baseFrequency;

    for (int z = 0; z < in.settings.depth; ++z)
    {
        for (int x = 0; x < in.settings.width; ++x)
        {
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

void applyRiverPass(TerrainFields& fields, const TerrainSettings& settings)
{
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

void computeHeightExtents(const std::vector<float>& heights, float& minHeight, float& maxHeight)
{
    minHeight = std::numeric_limits<float>::max();
    maxHeight = std::numeric_limits<float>::lowest();
    for (float height : heights)
    {
        minHeight = std::min(minHeight, height);
        maxHeight = std::max(maxHeight, height);
    }
}

HeightGradient sampleHeightGradient(
    const std::vector<float>& heights,
    int width,
    int depth,
    float horizontalScale,
    int x,
    int z)
{
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

void computeSlopeField(TerrainFields& fields, float horizontalScale)
{
    for (int z = 0; z < fields.depth; ++z)
    {
        for (int x = 0; x < fields.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, fields.width);
            fields.slopes[idx] =
                sampleHeightGradient(fields.heights, fields.width, fields.depth, horizontalScale, x, z).slope;
        }
    }
}

} // namespace

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

// Layers multiple noise, adds detail
float TerrainGenerator::fractalBrownianMotion(float x, float y, int octaves, float lacunarity, float gain) const
{
    return octaveNoise(x, y, octaves, lacunarity, gain, [](float n)
                       { return n; });
}

// Creates mountain ridges
float TerrainGenerator::ridgedFbm(
    float x,
    float y,
    int octaves,
    float lacunarity,
    float gain,
    float sharpness) const
{
    return octaveNoise(x, y, octaves, lacunarity, gain, [sharpness](float n)
                       { return std::pow(std::clamp(1.0f - std::fabs(n), 0.0f, 1.0f), sharpness); });
}

// Main generation function
// 1. Builds the scalar terrain fields (height, mountain mask, warped sample positions)
// 2. Applies post-processing stages (smoothing, rivers, slopes, climate, provinces, landforms, biomes)
// 3. Packs those fields into the renderable mesh
TerrainMesh TerrainGenerator::generateMesh() const
{
    const auto fbm = [this](float x, float y, int octaves, float lacunarity, float gain)
    {
        return this->fractalBrownianMotion(x, y, octaves, lacunarity, gain);
    };
    const auto ridged = [this](float x, float y, int octaves, float lacunarity, float gain, float sharpness)
    {
        return this->ridgedFbm(x, y, octaves, lacunarity, gain, sharpness);
    };

    TerrainFields fields = buildBaseTerrainFields({settings_, fbm, ridged});
    smoothHeights(fields.heights, fields.mountainWeights, settings_.width, settings_.depth);
    applyRiverPass(fields, settings_);
    computeSlopeField(fields, settings_.horizontalScale);
    computeClimateFields(fields, settings_, fbm);
    computeProvinceFields(fields, settings_);
    computeLandformFields(fields);
    computeBiomeFields(fields);

    TerrainMesh mesh;
    mesh.width = settings_.width;
    mesh.depth = settings_.depth;
    mesh.horizontalScale = settings_.horizontalScale;
    mesh.heights = fields.heights;
    mesh.temperatureMap = fields.temperature;
    mesh.precipitationMap = fields.precipitation;
    mesh.moistureMap = fields.moisture;
    computeHeightExtents(mesh.heights, mesh.minHeight, mesh.maxHeight);

    mesh.vertices.resize(static_cast<size_t>(settings_.width) * static_cast<size_t>(settings_.depth));

    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, settings_.width);

            TerrainVertex v;
            v.x = static_cast<float>(x) * settings_.horizontalScale;
            v.y = mesh.heights[idx];
            v.z = static_cast<float>(z) * settings_.horizontalScale;
            v.slope = fields.slopes[idx];
            v.mountainWeight = fields.mountainWeights[idx];
            v.plainsWeight = 1.0f - fields.mountainWeights[idx];
            v.riverWeight = fields.riverWeights[idx];
            v.temperature = fields.temperature[idx];
            v.precipitation = fields.precipitation[idx];
            v.moisture = fields.moisture[idx];
            v.provinceId = fields.provinceIds[idx];
            v.landform = fields.landformIds[idx];
            v.ecology = fields.ecologyIds[idx];
            v.primaryBiome = fields.primaryBiomeIds[idx];
            v.secondaryBiome = fields.secondaryBiomeIds[idx];
            v.primaryBiomeWeight = fields.primaryBiomeWeights[idx];
            v.secondaryBiomeWeight = fields.secondaryBiomeWeights[idx];
            mesh.vertices[idx] = v;
        }
    }

    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const HeightGradient gradient =
                sampleHeightGradient(mesh.heights, settings_.width, settings_.depth, settings_.horizontalScale, x, z);
            const size_t idx = fieldIndex(x, z, settings_.width);
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

    struct SettlementCandidate
    {
        size_t idx = 0;
        float score = 0.0f;
    };

    std::vector<SettlementCandidate> settlementCandidates;
    settlementCandidates.reserve(mesh.vertices.size() / 12u);
    const float invHeightRange = 1.0f / std::max(0.0001f, mesh.maxHeight - mesh.minHeight);
    const SettlementSettings& ss = settings_.settlements;

    // Build a distance field from the river core so settlements sit near water,
    // but not inside the carved river bed.
    std::vector<int> riverDistance(mesh.vertices.size(), std::numeric_limits<int>::max());
    std::vector<int> nearestCore(mesh.vertices.size(), -1);
    std::queue<size_t> q;

    for (size_t idx = 0; idx < mesh.vertices.size(); ++idx)
    {
        if (fields.riverWeights[idx] >= settings_.rivers.coreThreshold)
        {
            riverDistance[idx] = 0;
            nearestCore[idx] = static_cast<int>(idx);
            q.push(idx);
        }
    }

    const int neighborDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int neighborDz[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    while (!q.empty())
    {
        const size_t cur = q.front();
        q.pop();

        const int cx = static_cast<int>(cur % static_cast<size_t>(settings_.width));
        const int cz = static_cast<int>(cur / static_cast<size_t>(settings_.width));
        const int nextDist = riverDistance[cur] + 1;

        for (int i = 0; i < 8; ++i)
        {
            const int nx = cx + neighborDx[i];
            const int nz = cz + neighborDz[i];
            if (nx < 0 || nx >= settings_.width || nz < 0 || nz >= settings_.depth)
            {
                continue;
            }

            const size_t nidx = fieldIndex(nx, nz, settings_.width);
            if (nextDist >= riverDistance[nidx])
            {
                continue;
            }

            riverDistance[nidx] = nextDist;
            nearestCore[nidx] = nearestCore[cur];
            q.push(nidx);
        }
    }

    auto gatherCandidates = [&](float riverMin,
                                float riverMax,
                                int distMin,
                                int distMax,
                                float minAboveRiver,
                                float maxSlope)
    {
        settlementCandidates.clear();
        for (size_t idx = 0; idx < mesh.vertices.size(); ++idx)
        {
            const float river = std::clamp(fields.riverWeights[idx], 0.0f, 1.0f);
            if (river < riverMin || river > riverMax)
            {
                continue;
            }

            if (riverDistance[idx] < distMin || riverDistance[idx] > distMax)
            {
                continue;
            }

            if (nearestCore[idx] >= 0)
            {
                const size_t coreIdx = static_cast<size_t>(nearestCore[idx]);
                if (mesh.heights[idx] < mesh.heights[coreIdx] + minAboveRiver)
                {
                    continue;
                }
            }

            const float slope = fields.slopes[idx];
            if (slope > maxSlope)
            {
                continue;
            }

            const float hNorm = (mesh.heights[idx] - mesh.minHeight) * invHeightRange;
            if (hNorm < ss.minElevationNorm || hNorm > ss.maxElevationNorm)
            {
                continue;
            }

            const float flatness = 1.0f - std::clamp(slope / std::max(0.001f, maxSlope), 0.0f, 1.0f);
            const float elevationBand = 1.0f - std::fabs(hNorm - 0.42f);
            const float score =
                river * 0.58f +
                flatness * 0.34f +
                elevationBand * 0.08f +
                placementJitter(idx, settings_.seed) * 0.001f;
            settlementCandidates.push_back({idx, score});
        }
    };

    gatherCandidates(
        ss.minRiverWeight,
        ss.maxRiverWeight,
        ss.minRiverDistanceCells,
        ss.maxRiverDistanceCells,
        ss.minHeightAboveRiver,
        ss.maxSlope);

    if (settlementCandidates.size() < 24u)
    {
        gatherCandidates(
            std::max(0.0f, ss.minRiverWeight * 0.6f),
            std::min(1.0f, ss.maxRiverWeight + 0.2f),
            std::max(0, ss.minRiverDistanceCells - 1),
            ss.maxRiverDistanceCells + 6,
            std::max(0.0f, ss.minHeightAboveRiver * 0.35f),
            ss.maxSlope + 0.08f);
    }

    std::sort(settlementCandidates.begin(), settlementCandidates.end(), [](const SettlementCandidate& a, const SettlementCandidate& b)
              { return a.score > b.score; });

    const int desiredSettlements = std::max(
        1,
        static_cast<int>(std::round(ss.targetDensity * static_cast<float>(mesh.vertices.size()))));
    const int minSepSq = ss.minSeparation * ss.minSeparation;
    std::vector<size_t> selectedSettlements;
    selectedSettlements.reserve(static_cast<size_t>(desiredSettlements));
    
    // Build spatial grid for fast separation checks
    const int cellSize = std::max(1, ss.minSeparation * 2);
    const int gridWidth = (settings_.width + cellSize - 1) / cellSize;
    const int gridDepth = (settings_.depth + cellSize - 1) / cellSize;
    std::vector<std::vector<size_t>> grid(static_cast<size_t>(gridWidth * gridDepth));
    
    auto getGridCell = [gridWidth, cellSize](int x, int z) -> int {
        return (z / cellSize) * gridWidth + (x / cellSize);
    };
    
    for (const SettlementCandidate& candidate : settlementCandidates)
    {
        if (static_cast<int>(selectedSettlements.size()) >= desiredSettlements)
        {
            break;
        }
    
        const int cx = static_cast<int>(candidate.idx % static_cast<size_t>(settings_.width));
        const int cz = static_cast<int>(candidate.idx / static_cast<size_t>(settings_.width));
        bool tooClose = false;
    
        // Check only nearby grid cells
        const int cellX = cx / cellSize;
        const int cellZ = cz / cellSize;
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = cellX + dx;
                const int nz = cellZ + dz;
                if (nx < 0 || nx >= gridWidth || nz < 0 || nz >= gridDepth)
                {
                    continue;
                }
    
                const int cellIdx = nz * gridWidth + nx;
                for (size_t sidx : grid[cellIdx])
                {
                    const int sx = static_cast<int>(sidx % static_cast<size_t>(settings_.width));
                    const int sz = static_cast<int>(sidx / static_cast<size_t>(settings_.width));
                    const int dx_actual = cx - sx;
                    const int dz_actual = cz - sz;
                    if (dx_actual * dx_actual + dz_actual * dz_actual < minSepSq)
                    {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose)
                {
                    break;
                }
            }
            if (tooClose)
            {
                break;
            }
        }
    
        if (tooClose)
        {
            continue;
        }
        selectedSettlements.push_back(candidate.idx);
        const int cellIdx = getGridCell(cx, cz);
        grid[cellIdx].push_back(candidate.idx);
    }

    if (selectedSettlements.empty() && !settlementCandidates.empty())
    {
        selectedSettlements.push_back(settlementCandidates.front().idx);
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

    mesh.waterVertices.resize(mesh.vertices.size());
    for (int z = 0; z < settings_.depth; ++z)
    {
        for (int x = 0; x < settings_.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, settings_.width);
            TerrainVertex water = mesh.vertices[idx];
            const float w = std::clamp(fields.riverWeights[idx], 0.0f, 1.0f);
            water.y = mesh.heights[idx] + 0.06f + w * 0.12f;
            water.nx = 0.0f;
            water.ny = 1.0f;
            water.nz = 0.0f;
            water.riverWeight = w;
            mesh.waterVertices[idx] = water;
        }
    }

    constexpr float kWaterSurfaceThreshold = 0.02f;
    mesh.waterIndices.reserve(mesh.indices.size() / 8u);
    const auto hasWater = [&mesh](uint32_t idx)
    {
        return mesh.waterVertices[idx].riverWeight > kWaterSurfaceThreshold;
    };

    for (int z = 0; z < settings_.depth - 1; ++z)
    {
        for (int x = 0; x < settings_.width - 1; ++x)
        {
            const uint32_t i00 = static_cast<uint32_t>(z * settings_.width + x);
            const uint32_t i10 = static_cast<uint32_t>(z * settings_.width + (x + 1));
            const uint32_t i01 = static_cast<uint32_t>((z + 1) * settings_.width + x);
            const uint32_t i11 = static_cast<uint32_t>((z + 1) * settings_.width + (x + 1));

            if (!(hasWater(i00) || hasWater(i10) || hasWater(i01) || hasWater(i11)))
            {
                continue;
            }

            mesh.waterIndices.push_back(i00);
            mesh.waterIndices.push_back(i10);
            mesh.waterIndices.push_back(i01);

            mesh.waterIndices.push_back(i10);
            mesh.waterIndices.push_back(i11);
            mesh.waterIndices.push_back(i01);
        }
    }

    mesh.settlementVertices.reserve(selectedSettlements.size());
    const float settlementHeightOffset = 0.015f * settings_.verticalScale;
    for (size_t idx : selectedSettlements)
    {
        TerrainVertex marker = mesh.vertices[idx];
        marker.y += settlementHeightOffset;
        marker.riverWeight = 0.0f;
        mesh.settlementVertices.push_back(marker);
    }

    return mesh;
}

} // namespace terrain
