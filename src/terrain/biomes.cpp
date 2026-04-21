#include "biomes.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace terrain
{

namespace
{

struct NamedColor
{
    const char* name = "";
    BiomeColor color;
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
    {"Foothill", {0.61f, 0.55f, 0.31f}},
    {"Mountain", {0.52f, 0.50f, 0.47f}},
    {"Alpine", {0.72f, 0.71f, 0.68f}},
    {"Snowcap", {0.96f, 0.97f, 0.99f}},
}};

constexpr std::array<NamedColor, kBiomeCount> kBiomeInfo = {{
    {"Marsh lowland", {0.26f, 0.40f, 0.20f}},
    {"Desert plain", {0.82f, 0.75f, 0.50f}},
    {"Steppe plain", {0.68f, 0.61f, 0.29f}},
    {"Grassland plain", {0.51f, 0.66f, 0.28f}},
    {"Forest plain", {0.22f, 0.43f, 0.19f}},
    {"Taiga plain", {0.33f, 0.46f, 0.34f}},
    {"Tundra plain", {0.70f, 0.72f, 0.63f}},
    {"Steppe foothill", {0.58f, 0.52f, 0.31f}},
    {"Grassland foothill", {0.45f, 0.52f, 0.27f}},
    {"Forest foothill", {0.25f, 0.37f, 0.21f}},
    {"Taiga foothill", {0.34f, 0.41f, 0.37f}},
    {"Rocky mountain", {0.52f, 0.50f, 0.48f}},
    {"Alpine", {0.62f, 0.64f, 0.58f}},
    {"Snow", {0.95f, 0.97f, 0.99f}},
    {"River", {0.10f, 0.40f, 0.68f}},
}};

constexpr std::array<BiomeColor, 12> kProvincePalette = {{
    {0.78f, 0.34f, 0.29f},
    {0.27f, 0.58f, 0.76f},
    {0.48f, 0.66f, 0.28f},
    {0.71f, 0.47f, 0.77f},
    {0.84f, 0.61f, 0.25f},
    {0.22f, 0.67f, 0.55f},
    {0.68f, 0.52f, 0.32f},
    {0.58f, 0.36f, 0.58f},
    {0.33f, 0.43f, 0.77f},
    {0.73f, 0.29f, 0.47f},
    {0.42f, 0.66f, 0.62f},
    {0.67f, 0.58f, 0.26f},
}};

struct ProvinceAggregate
{
    float temperature = 0.0f;
    float moisture = 0.0f;
    float elevation = 0.0f;
    int cellCount = 0;
    EcologyId baseEcology = EcologyId::Grassland;
};

using BiomeWeightVector = std::array<float, kBiomeCount>;

size_t ecologyIndex(EcologyId ecology)
{
    return static_cast<size_t>(ecology);
}

size_t landformIndex(LandformId landform)
{
    return static_cast<size_t>(landform);
}

size_t biomeIndex(BiomeId biome)
{
    return static_cast<size_t>(biome);
}

void normalizeWeights(BiomeWeightVector& weights)
{
    float sum = 0.0f;
    for (float weight : weights)
    {
        sum += weight;
    }

    if (sum <= 0.0001f)
    {
        weights.fill(0.0f);
        weights[biomeIndex(BiomeId::GrasslandPlain)] = 1.0f;
        return;
    }

    const float invSum = 1.0f / sum;
    for (float& weight : weights)
    {
        weight *= invSum;
    }
}

BiomeWeightVector vertexBiomeWeights(const TerrainFields& fields, size_t idx)
{
    BiomeWeightVector weights{};
    weights.fill(0.0f);
    weights[fields.primaryBiomeIds[idx]] += fields.primaryBiomeWeights[idx];
    weights[fields.secondaryBiomeIds[idx]] += fields.secondaryBiomeWeights[idx];
    normalizeWeights(weights);
    return weights;
}

void writeBiomeWeights(TerrainFields& fields, size_t idx, const BiomeWeightVector& weights)
{
    size_t primary = biomeIndex(BiomeId::GrasslandPlain);
    size_t secondary = primary;
    float primaryWeight = 0.0f;
    float secondaryWeight = 0.0f;
    for (size_t biome = 0; biome < weights.size(); ++biome)
    {
        const float weight = weights[biome];
        if (weight > primaryWeight)
        {
            secondary = primary;
            secondaryWeight = primaryWeight;
            primary = biome;
            primaryWeight = weight;
        }
        else if (weight > secondaryWeight)
        {
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

float biomeNeighborCompatibility(const TerrainFields& fields, size_t idx, size_t nidx)
{
    const int landformDelta = std::abs(static_cast<int>(fields.landformIds[idx]) - static_cast<int>(fields.landformIds[nidx]));
    float compatibility = 1.0f;
    if (landformDelta == 1)
    {
        compatibility *= 0.84f;
    }
    else if (landformDelta >= 2)
    {
        compatibility *= 0.48f;
    }

    if (fields.provinceIds[idx] != fields.provinceIds[nidx])
    {
        compatibility *= 0.82f;
    }

    if (fields.ecologyIds[idx] != fields.ecologyIds[nidx])
    {
        compatibility *= 0.90f;
    }

    return compatibility;
}

void smoothSurfaceBiomeWeights(TerrainFields& fields)
{
    if (fields.size() == 0)
    {
        return;
    }

    std::vector<BiomeWeightVector> current(fields.size());
    std::vector<BiomeWeightVector> next(fields.size());
    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        current[idx] = vertexBiomeWeights(fields, idx);
    }

    for (int pass = 0; pass < 3; ++pass)
    {
        for (int z = 0; z < fields.depth; ++z)
        {
            const int z0 = std::max(0, z - 2);
            const int z1 = std::min(fields.depth - 1, z + 2);
            for (int x = 0; x < fields.width; ++x)
            {
                const size_t idx = fieldIndex(x, z, fields.width);
                next[idx].fill(0.0f);
                float totalWeight = 0.0f;

                for (int nz = z0; nz <= z1; ++nz)
                {
                    const int x0 = std::max(0, x - 2);
                    const int x1 = std::min(fields.width - 1, x + 2);
                    for (int nx = x0; nx <= x1; ++nx)
                    {
                        const size_t nidx = fieldIndex(nx, nz, fields.width);
                        const int manhattan = std::abs(nx - x) + std::abs(nz - z);
                        const float kernelWeight = std::max(1.0f, 6.0f - static_cast<float>(manhattan));
                        const float weight = kernelWeight * biomeNeighborCompatibility(fields, idx, nidx);
                        totalWeight += weight;
                        for (size_t biome = 0; biome < kBiomeCount; ++biome)
                        {
                            next[idx][biome] += current[nidx][biome] * weight;
                        }
                    }
                }

                if (totalWeight > 0.0001f)
                {
                    const float invWeight = 1.0f / totalWeight;
                    for (float& weight : next[idx])
                    {
                        weight *= invWeight;
                    }
                }
                normalizeWeights(next[idx]);
            }
        }

        current.swap(next);
    }

    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        writeBiomeWeights(fields, idx, current[idx]);
    }
}

EcologyId classifyProvinceEcology(float temperature, float moisture)
{
    if (temperature < 0.18f)
    {
        return EcologyId::Tundra;
    }
    if (temperature < 0.32f)
    {
        return moisture < 0.40f ? EcologyId::Tundra : EcologyId::Taiga;
    }
    if (temperature > 0.72f && moisture < 0.20f)
    {
        return EcologyId::Desert;
    }
    if (moisture < 0.34f)
    {
        return temperature > 0.60f ? EcologyId::Desert : EcologyId::Steppe;
    }
    if (moisture < 0.62f)
    {
        return EcologyId::Grassland;
    }
    if (temperature < 0.42f)
    {
        return EcologyId::Taiga;
    }
    return EcologyId::Forest;
}

EcologyId classifyLocalEcology(
    EcologyId baseEcology,
    LandformId landform,
    float temperature,
    float moisture,
    float riverWeight,
    float slope)
{
    if (landform == LandformId::Lowland &&
        riverWeight > 0.10f &&
        moisture > 0.62f &&
        slope < 0.10f)
    {
        return EcologyId::Marsh;
    }

    if (temperature < 0.18f)
    {
        return moisture > 0.44f ? EcologyId::Taiga : EcologyId::Tundra;
    }

    switch (baseEcology)
    {
    case EcologyId::Desert:
        return (moisture > 0.34f || riverWeight > 0.08f) ? EcologyId::Steppe : EcologyId::Desert;
    case EcologyId::Steppe:
        if (moisture > 0.60f)
        {
            return EcologyId::Grassland;
        }
        if (moisture < 0.16f && temperature > 0.72f)
        {
            return EcologyId::Desert;
        }
        return EcologyId::Steppe;
    case EcologyId::Grassland:
        if (temperature < 0.26f)
        {
            return moisture > 0.46f ? EcologyId::Taiga : EcologyId::Tundra;
        }
        if (moisture > 0.70f)
        {
            return EcologyId::Forest;
        }
        if (moisture < 0.24f && temperature > 0.56f)
        {
            return EcologyId::Steppe;
        }
        return EcologyId::Grassland;
    case EcologyId::Forest:
        if (temperature < 0.28f)
        {
            return EcologyId::Taiga;
        }
        if (moisture < 0.44f)
        {
            return EcologyId::Grassland;
        }
        return EcologyId::Forest;
    case EcologyId::Taiga:
        if (temperature < 0.18f || moisture < 0.28f)
        {
            return EcologyId::Tundra;
        }
        if (temperature > 0.46f && moisture < 0.42f)
        {
            return EcologyId::Grassland;
        }
        return EcologyId::Taiga;
    case EcologyId::Tundra:
        return (temperature > 0.24f && moisture > 0.48f) ? EcologyId::Taiga : EcologyId::Tundra;
    case EcologyId::Marsh:
        return slope < 0.10f && moisture > 0.60f ? EcologyId::Marsh : EcologyId::Grassland;
    default:
        return baseEcology;
    }
}

BiomeId mapBiome(LandformId landform, EcologyId ecology)
{
    if (landform == LandformId::Snowcap)
    {
        return BiomeId::Snow;
    }
    if (landform == LandformId::Alpine)
    {
        return BiomeId::Alpine;
    }
    if (landform == LandformId::Mountain)
    {
        return BiomeId::RockyAlpine;
    }
    if (landform == LandformId::Foothill)
    {
        switch (ecology)
        {
        case EcologyId::Forest:
            return BiomeId::ForestFoothill;
        case EcologyId::Taiga:
        case EcologyId::Tundra:
        case EcologyId::Marsh:
            return BiomeId::TaigaFoothill;
        case EcologyId::Grassland:
            return BiomeId::GrasslandFoothill;
        case EcologyId::Steppe:
        case EcologyId::Desert:
        default:
            return BiomeId::SteppeFoothill;
        }
    }

    switch (ecology)
    {
    case EcologyId::Marsh:
        return BiomeId::MarshLowland;
    case EcologyId::Desert:
        return BiomeId::DesertPlain;
    case EcologyId::Steppe:
        return BiomeId::SteppePlain;
    case EcologyId::Forest:
        return BiomeId::ForestPlain;
    case EcologyId::Taiga:
        return BiomeId::TaigaPlain;
    case EcologyId::Tundra:
        return BiomeId::TundraPlain;
    case EcologyId::Grassland:
    default:
        return BiomeId::GrasslandPlain;
    }
}

void smoothEcologyField(TerrainFields& fields)
{
    std::vector<uint8_t> smoothed = fields.ecologyIds;
    std::array<int, kEcologyCount> counts{};

    for (int z = 0; z < fields.depth; ++z)
    {
        const int z0 = std::max(0, z - 1);
        const int z1 = std::min(fields.depth - 1, z + 1);
        for (int x = 0; x < fields.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, fields.width);
            const LandformId landform = static_cast<LandformId>(fields.landformIds[idx]);
            const EcologyId current = static_cast<EcologyId>(fields.ecologyIds[idx]);
            if (landform > LandformId::Foothill || current == EcologyId::Marsh)
            {
                continue;
            }

            counts.fill(0);
            const uint16_t province = fields.provinceIds[idx];
            for (int nz = z0; nz <= z1; ++nz)
            {
                const int x0 = std::max(0, x - 1);
                const int x1 = std::min(fields.width - 1, x + 1);
                for (int nx = x0; nx <= x1; ++nx)
                {
                    const size_t nidx = fieldIndex(nx, nz, fields.width);
                    if (fields.provinceIds[nidx] != province)
                    {
                        continue;
                    }
                    const EcologyId neighbor = static_cast<EcologyId>(fields.ecologyIds[nidx]);
                    if (neighbor == EcologyId::Marsh)
                    {
                        continue;
                    }
                    ++counts[ecologyIndex(neighbor)];
                }
            }

            size_t best = ecologyIndex(current);
            for (size_t ecology = 0; ecology < counts.size(); ++ecology)
            {
                if (counts[ecology] > counts[best])
                {
                    best = ecology;
                }
            }
            if (counts[best] >= 5)
            {
                smoothed[idx] = static_cast<uint8_t>(best);
            }
        }
    }

    fields.ecologyIds.swap(smoothed);
}

EcologyId neighboringProvinceEcology(
    const TerrainFields& fields,
    const std::vector<ProvinceAggregate>& provinces,
    int x,
    int z)
{
    std::array<int, kEcologyCount> counts{};
    const size_t idx = fieldIndex(x, z, fields.width);
    const uint16_t province = fields.provinceIds[idx];

    const int z0 = std::max(0, z - 1);
    const int z1 = std::min(fields.depth - 1, z + 1);
    const int x0 = std::max(0, x - 1);
    const int x1 = std::min(fields.width - 1, x + 1);
    for (int nz = z0; nz <= z1; ++nz)
    {
        for (int nx = x0; nx <= x1; ++nx)
        {
            const size_t nidx = fieldIndex(nx, nz, fields.width);
            if (fields.provinceIds[nidx] == province)
            {
                continue;
            }
            const EcologyId neighborEcology = provinces[fields.provinceIds[nidx]].baseEcology;
            ++counts[ecologyIndex(neighborEcology)];
        }
    }

    size_t best = counts.size();
    for (size_t ecology = 0; ecology < counts.size(); ++ecology)
    {
        if (counts[ecology] == 0)
        {
            continue;
        }
        if (best == counts.size() || counts[ecology] > counts[best])
        {
            best = ecology;
        }
    }

    return best == counts.size() ? provinces[province].baseEcology : static_cast<EcologyId>(best);
}

void storeBiomeBlend(TerrainFields& fields, size_t idx, BiomeId a, float weightA, BiomeId b, float weightB)
{
    weightA = std::max(0.0f, weightA);
    weightB = std::max(0.0f, weightB);
    if (b == a || weightB <= 0.001f)
    {
        fields.primaryBiomeIds[idx] = static_cast<uint8_t>(a);
        fields.secondaryBiomeIds[idx] = static_cast<uint8_t>(a);
        fields.primaryBiomeWeights[idx] = 1.0f;
        fields.secondaryBiomeWeights[idx] = 0.0f;
        return;
    }

    if (weightB > weightA)
    {
        std::swap(a, b);
        std::swap(weightA, weightB);
    }

    const float sum = std::max(0.0001f, weightA + weightB);
    fields.primaryBiomeIds[idx] = static_cast<uint8_t>(a);
    fields.secondaryBiomeIds[idx] = static_cast<uint8_t>(b);
    fields.primaryBiomeWeights[idx] = weightA / sum;
    fields.secondaryBiomeWeights[idx] = weightB / sum;
}

struct TransitionChoice
{
    BiomeId biome = BiomeId::GrasslandPlain;
    float weight = 0.0f;
};

TransitionChoice landformTransition(
    LandformId landform,
    EcologyId ecology,
    float landformSignal,
    float elevationNorm,
    float temperature)
{
    switch (landform)
    {
    case LandformId::Lowland:
    case LandformId::Plain:
        return {mapBiome(LandformId::Foothill, ecology), 0.38f * smoothstep(0.34f, 0.48f, landformSignal)};
    case LandformId::Foothill:
        if (landformSignal < 0.40f)
        {
            return {mapBiome(LandformId::Plain, ecology), 0.30f * (1.0f - smoothstep(0.34f, 0.46f, landformSignal))};
        }
        return {BiomeId::RockyAlpine, 0.42f * smoothstep(0.58f, 0.72f, std::max(landformSignal, elevationNorm))};
    case LandformId::Mountain:
        return {BiomeId::Alpine, 0.34f * smoothstep(0.70f, 0.84f, std::max(elevationNorm, 1.0f - temperature * 0.65f))};
    case LandformId::Alpine:
        return {BiomeId::Snow, 0.44f * smoothstep(0.84f, 0.94f, elevationNorm) * smoothstep(0.20f, 0.48f, 1.0f - temperature)};
    case LandformId::Snowcap:
        return {BiomeId::Alpine, 0.18f};
    default:
        return {BiomeId::GrasslandPlain, 0.0f};
    }
}

} // namespace

void computeBiomeFields(TerrainFields& fields)
{
    if (fields.heights.empty())
    {
        return;
    }

    float minHeight, maxHeight;
    computeHeightExtents(fields.heights, minHeight, maxHeight);
    uint16_t maxProvinceId = 0u;
    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        maxProvinceId = std::max(maxProvinceId, fields.provinceIds[idx]);
    }
    const float invHeightRange = 1.0f / std::max(0.0001f, maxHeight - minHeight);

    std::vector<ProvinceAggregate> provinces(static_cast<size_t>(maxProvinceId) + 1u);
    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        const size_t provinceIdx = static_cast<size_t>(fields.provinceIds[idx]);
        ProvinceAggregate& province = provinces[provinceIdx];
        province.temperature += fields.temperature[idx];
        province.moisture += fields.moisture[idx];
        province.elevation += (fields.heights[idx] - minHeight) * invHeightRange;
        ++province.cellCount;
    }

    for (ProvinceAggregate& province : provinces)
    {
        if (province.cellCount <= 0)
        {
            province.baseEcology = EcologyId::Grassland;
            continue;
        }
        const float invCount = 1.0f / static_cast<float>(province.cellCount);
        province.temperature *= invCount;
        province.moisture *= invCount;
        province.elevation *= invCount;
        province.baseEcology = classifyProvinceEcology(province.temperature, province.moisture);
    }

    for (size_t idx = 0; idx < fields.size(); ++idx)
    {
        const EcologyId baseEcology = provinces[fields.provinceIds[idx]].baseEcology;
        fields.ecologyIds[idx] = static_cast<uint8_t>(classifyLocalEcology(
            baseEcology,
            static_cast<LandformId>(fields.landformIds[idx]),
            fields.temperature[idx],
            fields.moisture[idx],
            fields.riverWeights[idx],
            fields.slopes[idx]));
    }

    smoothEcologyField(fields);

    for (int z = 0; z < fields.depth; ++z)
    {
        for (int x = 0; x < fields.width; ++x)
        {
            const size_t idx = fieldIndex(x, z, fields.width);
            const LandformId landform = static_cast<LandformId>(fields.landformIds[idx]);
            const EcologyId ecology = static_cast<EcologyId>(fields.ecologyIds[idx]);
            const EcologyId provinceEcology = provinces[fields.provinceIds[idx]].baseEcology;
            const float elevationNorm = (fields.heights[idx] - minHeight) * invHeightRange;
            const BiomeId baseBiome = mapBiome(landform, ecology);

            const float riverBlend = (landform <= LandformId::Foothill)
                                         ? smoothstep(0.12f, 0.58f, std::clamp(fields.riverWeights[idx], 0.0f, 1.0f))
                                         : 0.0f;
            if (riverBlend > 0.02f)
            {
                storeBiomeBlend(fields, idx, baseBiome, 1.0f - riverBlend, BiomeId::River, riverBlend);
                continue;
            }

            if (ecology == EcologyId::Marsh)
            {
                const EcologyId underlyingEcology = provinceEcology == EcologyId::Marsh ? EcologyId::Grassland : provinceEcology;
                const float marshBlend =
                    smoothstep(0.58f, 0.86f, std::clamp(fields.moisture[idx], 0.0f, 1.0f)) *
                    (1.0f - smoothstep(0.08f, 0.22f, std::clamp(fields.slopes[idx], 0.0f, 1.0f))) *
                    smoothstep(0.05f, 0.24f, std::clamp(fields.riverWeights[idx] + 0.10f, 0.0f, 1.0f));
                storeBiomeBlend(fields, idx, mapBiome(LandformId::Lowland, underlyingEcology), 1.0f - marshBlend, BiomeId::MarshLowland, marshBlend);
                continue;
            }

            BiomeId secondaryBiome = baseBiome;
            float secondaryWeight = 0.0f;

            if (landform <= LandformId::Foothill)
            {
                const EcologyId edgeEcology = neighboringProvinceEcology(fields, provinces, x, z);
                const BiomeId edgeBiome = mapBiome(landform, edgeEcology);
                const float edgeBlend = 0.52f * (1.0f - smoothstep(2.0f, 15.0f, static_cast<float>(fields.provinceEdgeDistance[idx])));
                if (edgeBiome != baseBiome && edgeBlend > secondaryWeight)
                {
                    secondaryBiome = edgeBiome;
                    secondaryWeight = edgeBlend;
                }
            }

            const TransitionChoice landformChoice = landformTransition(
                landform,
                ecology,
                fields.landformSignal[idx],
                elevationNorm,
                fields.temperature[idx]);
            if (landformChoice.biome != baseBiome && landformChoice.weight > secondaryWeight)
            {
                secondaryBiome = landformChoice.biome;
                secondaryWeight = landformChoice.weight;
            }

            storeBiomeBlend(fields, idx, baseBiome, 1.0f - secondaryWeight, secondaryBiome, secondaryWeight);
        }
    }

    smoothSurfaceBiomeWeights(fields);
}

const char* biomeName(BiomeId biome)
{
    return kBiomeInfo[biomeIndex(biome)].name;
}

BiomeColor biomeColor(BiomeId biome)
{
    return kBiomeInfo[biomeIndex(biome)].color;
}

const char* ecologyName(EcologyId ecology)
{
    return kEcologyInfo[ecologyIndex(ecology)].name;
}

BiomeColor ecologyColor(EcologyId ecology)
{
    return kEcologyInfo[ecologyIndex(ecology)].color;
}

const char* landformName(LandformId landform)
{
    return kLandformInfo[landformIndex(landform)].name;
}

BiomeColor landformColor(LandformId landform)
{
    return kLandformInfo[landformIndex(landform)].color;
}

BiomeColor provinceColor(uint16_t provinceId)
{
    const BiomeColor base = kProvincePalette[static_cast<size_t>(provinceId) % kProvincePalette.size()];
    const float tint = 0.92f + 0.08f * static_cast<float>((provinceId / static_cast<uint16_t>(kProvincePalette.size())) % 3u);
    return {
        std::clamp(base.r * tint, 0.0f, 1.0f),
        std::clamp(base.g * tint, 0.0f, 1.0f),
        std::clamp(base.b * tint, 0.0f, 1.0f),
    };
}

} // namespace terrain
