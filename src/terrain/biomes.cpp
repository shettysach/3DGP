#include "biomes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "util.h"
#include "voronoi.h"
#include "wfc.h"

namespace terrain {

namespace {

    struct NamedColor {
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

    inline void
    normalizeWeightArray(float* weights, size_t size, size_t defaultIdx) {
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
        normalizeWeightArray(
            weights.data(),
            weights.size(),
            biomeIndex(BiomeId::GrasslandPlain)
        );
    }

    BiomeWeightVector
    vertexBiomeWeights(const TerrainFields& fields, size_t idx) {
        BiomeWeightVector weights {};
        weights.fill(0.0f);
        weights[fields.primaryBiomeIds[idx]] += fields.primaryBiomeWeights[idx];
        weights[fields.secondaryBiomeIds[idx]] +=
            fields.secondaryBiomeWeights[idx];
        normalizeWeights(weights);
        return weights;
    }

    void writeBiomeWeights(
        TerrainFields& fields,
        size_t idx,
        const BiomeWeightVector& weights
    ) {
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

    float biomeNeighborCompatibility(
        const TerrainFields& fields,
        size_t idx,
        size_t nidx
    ) {
        const float temperatureDelta =
            std::fabs(fields.temperature[idx] - fields.temperature[nidx]);
        const float moistureDelta =
            std::fabs(fields.moisture[idx] - fields.moisture[nidx]);
        const float precipitationDelta =
            std::fabs(fields.precipitation[idx] - fields.precipitation[nidx]);
        const float slopeDelta =
            std::fabs(fields.slopes[idx] - fields.slopes[nidx]);
        const int landformDelta = std::abs(
            static_cast<int>(fields.landformIds[idx])
            - static_cast<int>(fields.landformIds[nidx])
        );

        const float climateCompatibility =
            (1.0f - smoothstep(0.08f, 0.40f, temperatureDelta)) * 0.34f
            + (1.0f - smoothstep(0.10f, 0.45f, moistureDelta)) * 0.38f
            + (1.0f - smoothstep(0.10f, 0.50f, precipitationDelta)) * 0.18f
            + (1.0f - smoothstep(0.08f, 0.35f, slopeDelta)) * 0.10f;

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
                            const size_t nidx =
                                fieldIndex(nx, nz, fields.width);
                            const int manhattan =
                                std::abs(nx - x) + std::abs(nz - z);
                            const float kernelWeight = std::max(
                                1.0f,
                                6.0f - static_cast<float>(manhattan)
                            );
                            const float weight = kernelWeight
                                * biomeNeighborCompatibility(fields, idx, nidx);
                            totalWeight += weight;
                            for (size_t biome = 0; biome < kBiomeCount;
                                 ++biome) {
                                next[idx][biome] +=
                                    current[nidx][biome] * weight;
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

    void forceHardBiomeLabels(TerrainFields& fields) {
        for (size_t idx = 0; idx < fields.size(); ++idx) {
            const uint8_t biome = fields.primaryBiomeIds[idx];
            fields.primaryBiomeIds[idx] = biome;
            fields.secondaryBiomeIds[idx] = biome;
            fields.primaryBiomeWeights[idx] = 1.0f;
            fields.secondaryBiomeWeights[idx] = 0.0f;
        }
    }

    void aggressiveBiomeCleanup(TerrainFields& fields) {
        std::vector<uint8_t> ids = fields.primaryBiomeIds;
        fields.primaryBiomeIds = ids;
        forceHardBiomeLabels(fields);
    }

} // namespace

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

void computeBiomeFieldsWFC(
    TerrainFields& fields,
    const TerrainSettings& settings
) {
    if (fields.heights.empty()) {
        return;
    }

    // 1. Generate Voronoi Graph
    std::cout << "[WFC] Generating Voronoi graph with cell size "
              << settings.voronoiCellSize << "..." << std::endl;
    VoronoiGraph graph(fields.width, fields.depth, settings.voronoiCellSize);

    // 2. Prepare Heuristics (average climate per cell)
    size_t cellCount = graph.cells().size();
    std::cout << "[WFC] Created " << cellCount
              << " Voronoi territories. Preparing climate heuristics..."
              << std::endl;
    std::vector<float> tempHeuristics(cellCount, 0.0f);
    std::vector<float> moistureHeuristics(cellCount, 0.0f);

    for (size_t cIdx = 0; cIdx < cellCount; ++cIdx) {
        const auto& cell = graph.cells()[cIdx];
        if (cell.gridIndices.empty())
            continue;

        float tSum = 0.0f;
        float mSum = 0.0f;
        for (size_t gIdx : cell.gridIndices) {
            tSum += fields.temperature[gIdx];
            mSum += fields.moisture[gIdx];
        }
        tempHeuristics[cIdx] =
            tSum / static_cast<float>(cell.gridIndices.size());
        moistureHeuristics[cIdx] =
            mSum / static_cast<float>(cell.gridIndices.size());
    }

    // 3. Solve WFC
    std::cout << "[WFC] Starting Wave Function Collapse solver..." << std::endl;
    BiomeConstraintGraph constraints;
    WFCBiomeSolver solver(graph, constraints, settings.seed);
    // check compuet biome fields.
    // We'll pass heuristics to help selection (though current solver doesn't use them yet,
    // it's ready for that expansion)
    if (!solver.solve(tempHeuristics, moistureHeuristics)) {
        throw std::runtime_error(
            "[WFC] ERROR: Solver hit a contradiction it couldn't resolve!"
        );
    }
    std::cout
        << "[WFC] SUCCESS! Map solved using constraint propagation and backtracking."
        << std::endl;
    std::cout << "[WFC] Mapping regional biomes back to high-res pixel grid..."
              << std::endl;
    // 4. Map results back to grid with Voronoi distance-based interpolation
    const float blendWidth = 16.0f; // Width of the transition zone
    const auto& cells = graph.cells();
    const auto& gridToCellMap = graph.gridToCellMap();

    for (int z = 0; z < fields.depth; ++z) {
        for (int x = 0; x < fields.width; ++x) {
            float fx = static_cast<float>(x);
            float fz = static_cast<float>(z);
            size_t gIdx = static_cast<size_t>(z * fields.width + x);
            
            uint32_t bestCell = gridToCellMap[gIdx];
            float cx1 = cells[bestCell].centerX;
            float cz1 = cells[bestCell].centerZ;
            float d1 = std::sqrt((fx - cx1) * (fx - cx1) + (fz - cz1) * (fz - cz1));
            
            float d2 = 1e18f;
            uint32_t secondBestCell = bestCell;
            
            for (uint32_t nIdx : cells[bestCell].neighborIndices) {
                float cx2 = cells[nIdx].centerX;
                float cz2 = cells[nIdx].centerZ;
                float dist = std::sqrt((fx - cx2) * (fx - cx2) + (fz - cz2) * (fz - cz2));
                if (dist < d2) {
                    d2 = dist;
                    secondBestCell = nIdx;
                }
            }
            
            BiomeId primary = solver.getResult(bestCell);
            BiomeId secondary = solver.getResult(secondBestCell);
            
            float primaryWeight = 1.0f;
            float secondaryWeight = 0.0f;
            
            if (primary != secondary && d2 < 1e10f) {
                float delta = d2 - d1;
                float t = std::clamp(delta / blendWidth, 0.0f, 1.0f);
                float blend = t * t * (3.0f - 2.0f * t); // smoothstep
                primaryWeight = 0.5f + 0.5f * blend;
                secondaryWeight = 1.0f - primaryWeight;
            }
            
            fields.primaryBiomeIds[gIdx] = static_cast<uint8_t>(primary);
            fields.secondaryBiomeIds[gIdx] = static_cast<uint8_t>(secondary);
            fields.primaryBiomeWeights[gIdx] = primaryWeight;
            fields.secondaryBiomeWeights[gIdx] = secondaryWeight;
        }
    }
}

} // namespace terrain
