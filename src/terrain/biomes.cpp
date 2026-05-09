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

    constexpr size_t kBiomeCount = static_cast<size_t>(BiomeId::Count);

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

} // namespace

const char* biomeName(BiomeId biome) {
    return kBiomeInfo[static_cast<size_t>(biome)].name;
}

BiomeColor biomeColor(BiomeId biome) {
    return kBiomeInfo[static_cast<size_t>(biome)].color;
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
            float d1 =
                std::sqrt((fx - cx1) * (fx - cx1) + (fz - cz1) * (fz - cz1));

            float d2 = 1e18f;
            uint32_t secondBestCell = bestCell;

            for (uint32_t nIdx : cells[bestCell].neighborIndices) {
                float cx2 = cells[nIdx].centerX;
                float cz2 = cells[nIdx].centerZ;
                float dist = std::sqrt(
                    (fx - cx2) * (fx - cx2) + (fz - cz2) * (fz - cz2)
                );
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
