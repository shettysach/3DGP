#include "biomes.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace terrain {

namespace {

struct NamedColor {
    const char* name = "";
    BiomeColor color;
};

struct EcologyClimateRule {
    EcologyId ecology = EcologyId::Grassland;
    float temperatureCenter = 0.5f;
    float moistureCenter = 0.5f;
    float temperatureSigma = 0.25f;
    float moistureSigma = 0.25f;
};

constexpr size_t kEcologyCount = static_cast<size_t>(EcologyId::Count);
constexpr size_t kLandformCount = static_cast<size_t>(LandformId::Count);
constexpr size_t kBiomeCount = static_cast<size_t>(BiomeId::Count);

constexpr std::array<NamedColor, kEcologyCount> kEcologyInfo = {{
    {"Desert", {0.82f, 0.75f, 0.50f}},
    {"Steppe", {0.68f, 0.61f, 0.29f}},
    {"Grassland", {0.51f, 0.66f, 0.28f}},
    {"Forest", {0.22f, 0.43f, 0.19f}},
    {"Taiga", {0.33f, 0.46f, 0.34f}},
    {"Tundra", {0.70f, 0.72f, 0.63f}},
    {"Marsh", {0.24f, 0.40f, 0.24f}},
}};

constexpr std::array<NamedColor, kLandformCount> kLandformInfo = {{
    {"Lowland", {0.23f, 0.53f, 0.38f}},
    {"Plain", {0.55f, 0.67f, 0.33f}},
    {"Valley", {0.34f, 0.49f, 0.26f}},
    {"Plateau", {0.60f, 0.52f, 0.35f}},
    {"Foothill", {0.61f, 0.55f, 0.31f}},
    {"Mountain", {0.52f, 0.50f, 0.47f}},
    {"Alpine", {0.72f, 0.71f, 0.68f}},
    {"Snowcap", {0.96f, 0.97f, 0.99f}},
}};

constexpr std::array<NamedColor, kBiomeCount> kBiomeInfo = {{
    {"Marsh lowland", {0.22f, 0.41f, 0.31f}},
    {"Desert plain", {0.86f, 0.74f, 0.48f}},
    {"Steppe plain", {0.76f, 0.62f, 0.24f}},
    {"Grassland plain", {0.57f, 0.69f, 0.24f}},
    {"Forest plain", {0.14f, 0.33f, 0.12f}},
    {"Taiga plain", {0.28f, 0.44f, 0.49f}},
    {"Tundra plain", {0.76f, 0.75f, 0.80f}},
    {"Steppe foothill", {0.66f, 0.50f, 0.24f}},
    {"Grassland foothill", {0.46f, 0.58f, 0.18f}},
    {"Forest foothill", {0.12f, 0.26f, 0.10f}},
    {"Taiga foothill", {0.24f, 0.35f, 0.42f}},
    {"Desert plateau", {0.80f, 0.62f, 0.38f}},
    {"Steppe plateau", {0.68f, 0.55f, 0.26f}},
    {"Grassland plateau", {0.50f, 0.60f, 0.22f}},
    {"Forest plateau", {0.18f, 0.36f, 0.16f}},
    {"Taiga plateau", {0.30f, 0.40f, 0.38f}},
    {"Tundra plateau", {0.72f, 0.73f, 0.76f}},
    {"Rocky mountain", {0.50f, 0.48f, 0.46f}},
    {"Alpine", {0.80f, 0.82f, 0.85f}},
    {"Snow", {0.98f, 0.99f, 1.00f}},
}};

constexpr std::array<EcologyClimateRule, kEcologyCount> kEcologyClimateRules = {{
    {EcologyId::Desert, 0.82f, 0.10f, 0.32f, 0.18f},
    {EcologyId::Steppe, 0.66f, 0.26f, 0.30f, 0.22f},
    {EcologyId::Grassland, 0.54f, 0.48f, 0.28f, 0.24f},
    {EcologyId::Forest, 0.56f, 0.72f, 0.26f, 0.22f},
    {EcologyId::Taiga, 0.28f, 0.58f, 0.22f, 0.26f},
    {EcologyId::Tundra, 0.12f, 0.34f, 0.20f, 0.24f},
    {EcologyId::Marsh, 0.44f, 0.88f, 0.22f, 0.16f},
}};

constexpr std::array<BiomeId, kEcologyCount> kPlainBiomeByEcology = {{
    BiomeId::DesertPlain,
    BiomeId::SteppePlain,
    BiomeId::GrasslandPlain,
    BiomeId::ForestPlain,
    BiomeId::TaigaPlain,
    BiomeId::TundraPlain,
    BiomeId::MarshLowland,
}};

constexpr std::array<BiomeId, kEcologyCount> kFoothillBiomeByEcology = {{
    BiomeId::SteppeFoothill,
    BiomeId::SteppeFoothill,
    BiomeId::GrasslandFoothill,
    BiomeId::ForestFoothill,
    BiomeId::TaigaFoothill,
    BiomeId::TaigaFoothill,
    BiomeId::TaigaFoothill,
}};

constexpr std::array<BiomeId, kEcologyCount> kPlateauBiomeByEcology = {{
    BiomeId::DesertPlateau,
    BiomeId::SteppePlateau,
    BiomeId::GrasslandPlateau,
    BiomeId::ForestPlateau,
    BiomeId::TaigaPlateau,
    BiomeId::TundraPlateau,
    BiomeId::TundraPlateau,
}};

using BiomeWeightVector = std::array<float, kBiomeCount>;
using EcologyWeightVector = std::array<float, kEcologyCount>;

size_t ecologyIndex(EcologyId ecology) {
    return static_cast<size_t>(ecology);
}

size_t landformIndex(LandformId landform) {
    return static_cast<size_t>(landform);
}

size_t biomeIndex(BiomeId biome) {
    return static_cast<size_t>(biome);
}

inline void normalizeWeightArray(float* weights, size_t size, size_t defaultIdx) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        sum += weights[i];
    }
    if (sum <= 0.0001f) {
        std::fill(weights, weights + size, 0.0f);
        weights[defaultIdx] = 1.0f;
        return;
    }
    const float invSum = 1.0f / sum;
    for (size_t i = 0; i < size; ++i) {
        weights[i] *= invSum;
    }
}

void normalizeWeights(BiomeWeightVector& weights) {
    normalizeWeightArray(weights.data(), weights.size(), biomeIndex(BiomeId::GrasslandPlain));
}

void normalizeWeights(EcologyWeightVector& weights) {
    normalizeWeightArray(weights.data(), weights.size(), ecologyIndex(EcologyId::Grassland));
}

float gaussian2(float x, float y, float cx, float cy, float sx, float sy) {
    const float nx = (x - cx) / std::max(0.001f, sx);
    const float ny = (y - cy) / std::max(0.001f, sy);
    return std::exp(-(nx * nx + ny * ny));
}

void applyMappedEcologyWeights(
    BiomeWeightVector& biomeWeights,
    const EcologyWeightVector& ecologyWeights,
    const std::array<BiomeId, kEcologyCount>& mapping) {
    const size_t count = ecologyWeights.size();
    for (size_t ecology = 0; ecology < count; ecology++) {
        biomeWeights[biomeIndex(mapping[ecology])] += ecologyWeights[ecology];
    }
}

EcologyWeightVector ecologyWeightsFromClimate(
    float temperature,
    float moisture,
    float slope,
    float riverWeight,
    float mountainWeight,
    LandformId landform) {
    EcologyWeightVector weights{};
    weights.fill(0.0f);

    const float t = std::clamp(temperature, 0.0f, 1.0f);
    const float m = std::clamp(moisture, 0.0f, 1.0f);
    const float s = std::clamp(slope, 0.0f, 1.0f);
    const float r = std::clamp(riverWeight, 0.0f, 1.0f);
    const float mw = std::clamp(mountainWeight, 0.0f, 1.0f);

    const float cold = 1.0f - smoothstep(0.18f, 0.42f, t);
    const float hot = smoothstep(0.62f, 0.85f, t);
    const float dry = 1.0f - smoothstep(0.20f, 0.55f, m);
    const float wet = smoothstep(0.55f, 0.85f, m);
    const float flat = 1.0f - std::clamp(s / 0.25f, 0.0f, 1.0f);

    const float mountainMoistureBonus = mw * 0.35f;
    const float effectiveMoisture = std::clamp(m + mountainMoistureBonus, 0.0f, 1.0f);

    for (const EcologyClimateRule& rule : kEcologyClimateRules) {
        weights[ecologyIndex(rule.ecology)] = gaussian2(
            t,
            effectiveMoisture,
            rule.temperatureCenter,
            rule.moistureCenter,
            rule.temperatureSigma,
            rule.moistureSigma);
    }

    weights[ecologyIndex(EcologyId::Desert)] += hot * dry * 0.48f;
    weights[ecologyIndex(EcologyId::Steppe)] += dry * (1.0f - hot * 0.50f) * 0.28f;
    weights[ecologyIndex(EcologyId::Grassland)] += (1.0f - dry) * (1.0f - wet * 0.55f) * (1.0f - cold * 0.70f) * 0.18f;
    weights[ecologyIndex(EcologyId::Forest)] += wet * (1.0f - cold * 0.50f) * 0.30f;
    weights[ecologyIndex(EcologyId::Taiga)] += cold * wet * 0.42f;
    weights[ecologyIndex(EcologyId::Tundra)] += cold * (1.0f - smoothstep(0.35f, 0.90f, effectiveMoisture)) * 0.40f;

    if (landform <= LandformId::Foothill) {
        weights[ecologyIndex(EcologyId::Marsh)] +=
            flat * wet * smoothstep(0.06f, 0.40f, r) * smoothstep(0.03f, 0.30f, m) * 0.85f;
    }

    normalizeWeights(weights);
    return weights;
}

EcologyId dominantEcology(const EcologyWeightVector& weights) {
    size_t best = ecologyIndex(EcologyId::Grassland);
    for (size_t idx = 0; idx < weights.size(); ++idx) {
        if (weights[idx] > weights[best]) {
            best = idx;
        }
    }
    return static_cast<EcologyId>(best);
}

BiomeWeightVector biomeWeightsFromEcology(
    LandformId landform,
    const EcologyWeightVector& ecology,
    float elevationNorm,
    float temperature,
    float moisture) {
    BiomeWeightVector weights{};
    weights.fill(0.0f);

    const auto eco = [&ecology](EcologyId id) {
        return ecology[ecologyIndex(id)];
    };

    if (landform == LandformId::Snowcap) {
        weights[biomeIndex(BiomeId::Snow)] = 1.0f;
        return weights;
    }

    if (landform == LandformId::Alpine) {
        const float snowWeight =
            smoothstep(0.80f, 0.95f, std::clamp(elevationNorm, 0.0f, 1.0f)) *
            smoothstep(0.28f, 0.62f, 1.0f - std::clamp(temperature, 0.0f, 1.0f));
        weights[biomeIndex(BiomeId::Alpine)] = 1.0f - snowWeight;
        weights[biomeIndex(BiomeId::Snow)] = snowWeight;
        normalizeWeights(weights);
        return weights;
    }

    if (landform == LandformId::Mountain) {
        const float alpineWeight =
            0.25f * smoothstep(0.72f, 0.92f, std::clamp(elevationNorm, 0.0f, 1.0f)) +
            0.20f * eco(EcologyId::Tundra) +
            0.15f * eco(EcologyId::Taiga);
        weights[biomeIndex(BiomeId::RockyAlpine)] = 1.0f - std::clamp(alpineWeight, 0.0f, 0.55f);
        weights[biomeIndex(BiomeId::Alpine)] = std::clamp(alpineWeight, 0.0f, 0.55f);
        normalizeWeights(weights);
        return weights;
    }

    if (landform == LandformId::Valley) {
        applyMappedEcologyWeights(weights, ecology, kPlainBiomeByEcology);
        const float wetValleyBoost = smoothstep(0.42f, 0.85f, std::clamp(moisture, 0.0f, 1.0f));
        weights[biomeIndex(BiomeId::MarshLowland)] += eco(EcologyId::Marsh) * 0.80f + wetValleyBoost * 0.15f;
        weights[biomeIndex(BiomeId::ForestPlain)] += eco(EcologyId::Forest) * 0.16f;
        weights[biomeIndex(BiomeId::GrasslandPlain)] += eco(EcologyId::Grassland) * 0.12f;
        normalizeWeights(weights);
        return weights;
    }

    if (landform == LandformId::Foothill) {
        applyMappedEcologyWeights(weights, ecology, kFoothillBiomeByEcology);
        normalizeWeights(weights);
        return weights;
    }

    if (landform == LandformId::Plateau) {
        applyMappedEcologyWeights(weights, ecology, kPlateauBiomeByEcology);
        const float dryBonus = smoothstep(0.55f, 0.85f, 1.0f - std::clamp(moisture, 0.0f, 1.0f));
        weights[biomeIndex(BiomeId::DesertPlateau)] += dryBonus * 0.25f;
        normalizeWeights(weights);
        return weights;
    }

    applyMappedEcologyWeights(weights, ecology, kPlainBiomeByEcology);

    const float wetLowlandBoost =
        smoothstep(0.62f, 0.90f, std::clamp(moisture, 0.0f, 1.0f)) *
        smoothstep(0.18f, 0.55f, std::clamp(temperature, 0.0f, 1.0f));
    const float marshWeight = eco(EcologyId::Marsh) * (landform == LandformId::Lowland ? 1.0f : 0.65f) +
                              wetLowlandBoost * (landform == LandformId::Lowland ? 0.18f : 0.08f);
    weights[biomeIndex(BiomeId::MarshLowland)] += marshWeight;

    normalizeWeights(weights);
    return weights;
}

BiomeWeightVector vertexBiomeWeights(const TerrainFields& fields, size_t idx) {
    BiomeWeightVector weights{};
    weights.fill(0.0f);
    weights[fields.primaryBiomeIds[idx]] += fields.primaryBiomeWeights[idx];
    weights[fields.secondaryBiomeIds[idx]] += fields.secondaryBiomeWeights[idx];
    normalizeWeights(weights);
    return weights;
}

void writeBiomeWeights(TerrainFields& fields, size_t idx, const BiomeWeightVector& weights) {
    size_t primary = biomeIndex(BiomeId::GrasslandPlain);
    size_t secondary = primary;
    float primaryWeight = 0.0f;
    float secondaryWeight = 0.0f;
    for (size_t biome = 0; biome < weights.size(); biome++) {
        const float weight = weights[biome];
        if (weight > primaryWeight) {
            secondary = primary;
            secondaryWeight = primaryWeight;
            primary = biome;
            primaryWeight = weight;
        } else if (weight > secondaryWeight) {
            secondary = biome;
            secondaryWeight = weight;
        }
    }

    const float sum = std::max(0.0001f, primaryWeight + secondaryWeight);
    fields.primaryBiomeIds[idx] = static_cast<uint8_t>(primary);
    fields.secondaryBiomeIds[idx] = static_cast<uint8_t>(secondary);
    fields.primaryBiomeWeights[idx] = primaryWeight / sum;
    fields.secondaryBiomeWeights[idx] = secondaryWeight / sum;
}

float biomeNeighborCompatibility(const TerrainFields& fields, size_t idx, size_t nidx) {
    const float temperatureDelta = std::fabs(fields.temperature[idx] - fields.temperature[nidx]);
    const float moistureDelta = std::fabs(fields.moisture[idx] - fields.moisture[nidx]);
    const float precipitationDelta = std::fabs(fields.precipitation[idx] - fields.precipitation[nidx]);
    const float slopeDelta = std::fabs(fields.slopes[idx] - fields.slopes[nidx]);
    const int landformDelta = std::abs(static_cast<int>(fields.landformIds[idx]) - static_cast<int>(fields.landformIds[nidx]));

    const float climateCompatibility =
        (1.0f - smoothstep(0.08f, 0.40f, temperatureDelta)) * 0.34f +
        (1.0f - smoothstep(0.10f, 0.45f, moistureDelta)) * 0.38f +
        (1.0f - smoothstep(0.10f, 0.50f, precipitationDelta)) * 0.18f +
        (1.0f - smoothstep(0.08f, 0.35f, slopeDelta)) * 0.10f;

    float compatibility = std::clamp(climateCompatibility, 0.18f, 1.0f);
    if (landformDelta == 1) {
        compatibility *= 0.84f;
    } else if (landformDelta >= 2) {
        compatibility *= 0.48f;
    }

    return compatibility;
}

void smoothSurfaceBiomeWeights(TerrainFields& fields) {
    const size_t cellCount = fields.size();
    if (cellCount == 0) {
        return;
    }

    std::vector<BiomeWeightVector> current(cellCount);
    std::vector<BiomeWeightVector> next(cellCount);
    for (size_t idx = 0; idx < cellCount; ++idx) {
        current[idx] = vertexBiomeWeights(fields, idx);
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (int z = 0; z < fields.depth; ++z) {
            const int z0 = std::max(0, z - 2);
            const int z1 = std::min(fields.depth - 1, z + 2);
            for (int x = 0; x < fields.width; ++x) {
                const size_t idx = fieldIndex(x, z, fields.width);
                next[idx].fill(0.0f);
                float totalWeight = 0.0f;

                for (int nz = z0; nz <= z1; ++nz) {
                    const int x0 = std::max(0, x - 2);
                    const int x1 = std::min(fields.width - 1, x + 2);
                    for (int nx = x0; nx <= x1; ++nx) {
                        const size_t nidx = fieldIndex(nx, nz, fields.width);
                        const int manhattan = std::abs(nx - x) + std::abs(nz - z);
                        const float kernelWeight = std::max(1.0f, 6.0f - static_cast<float>(manhattan));
                        const float weight = kernelWeight * biomeNeighborCompatibility(fields, idx, nidx);
                        totalWeight += weight;
                        for (size_t biome = 0; biome < kBiomeCount; ++biome) {
                            next[idx][biome] += current[nidx][biome] * weight;
                        }
                    }
                }

                if (totalWeight > 0.0001f) {
                    const float invWeight = 1.0f / totalWeight;
                    for (float& weight : next[idx]) {
                        weight *= invWeight;
                    }
                }
                normalizeWeights(next[idx]);
            }
        }

        current.swap(next);
    }

    for (size_t idx = 0; idx < cellCount; ++idx) {
        writeBiomeWeights(fields, idx, current[idx]);
    }
}

} // namespace

void computeBiomeFields(TerrainFields& fields) {
    if (fields.heights.empty()) {
        return;
    }

    const float invHeightRange = 1.0f / std::max(0.0001f, fields.maxHeight - fields.minHeight);
    const size_t cellCount = fields.size();

    for (size_t idx = 0; idx < cellCount; ++idx) {
        const LandformId landform = static_cast<LandformId>(fields.landformIds[idx]);
        const EcologyWeightVector ecologyWeights = ecologyWeightsFromClimate(
            fields.temperature[idx],
            fields.moisture[idx],
            fields.slopes[idx],
            fields.riverWeights[idx],
            fields.mountainWeights[idx],
            landform);
        fields.ecologyIds[idx] = static_cast<uint8_t>(dominantEcology(ecologyWeights));

        const float elevationNorm = (fields.heights[idx] - fields.minHeight) * invHeightRange;
        BiomeWeightVector biomeWeights = biomeWeightsFromEcology(
            landform,
            ecologyWeights,
            elevationNorm,
            fields.temperature[idx],
            fields.moisture[idx]);
        writeBiomeWeights(fields, idx, biomeWeights);
    }

    smoothSurfaceBiomeWeights(fields);
}

const char* biomeName(BiomeId biome) {
    return kBiomeInfo[biomeIndex(biome)].name;
}

BiomeColor biomeColor(BiomeId biome) {
    return kBiomeInfo[biomeIndex(biome)].color;
}

const char* ecologyName(EcologyId ecology) {
    return kEcologyInfo[ecologyIndex(ecology)].name;
}

BiomeColor ecologyColor(EcologyId ecology) {
    return kEcologyInfo[ecologyIndex(ecology)].color;
}

const char* landformName(LandformId landform) {
    return kLandformInfo[landformIndex(landform)].name;
}

BiomeColor landformColor(LandformId landform) {
    return kLandformInfo[landformIndex(landform)].color;
}

} // namespace terrain
