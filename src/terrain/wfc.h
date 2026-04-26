#ifndef TERRAIN_WFC_H
#define TERRAIN_WFC_H

#include "voronoi.h"
#include "terrain.h"
#include <vector>
#include <set>
#include <map>
#include <random>
#include <stack>

namespace terrain {

class BiomeConstraintGraph {
public:
    BiomeConstraintGraph();
    bool isCompatible(BiomeId a, BiomeId b) const;
    const std::vector<BiomeId>& getCompatible(BiomeId a) const;

private:
    void addRule(BiomeId a, BiomeId b);
    std::map<BiomeId, std::vector<BiomeId>> adjacencyRules_;
};

struct WFCState {
    std::vector<bool> possibilities; // flattened: [cellIdx * biomeCount + biomeId]
    std::vector<int8_t> collapsedBiomes; // -1 if uncollapsed
    int lastChoiceCell = -1;
    int lastChoiceBiome = -1;
};

class WFCBiomeSolver {
public:
    WFCBiomeSolver(const VoronoiGraph& graph, const BiomeConstraintGraph& constraints, uint32_t seed);

    // Run the solver. Returns true if successful.
    bool solve(const std::vector<float>& temperatureHeuristics, 
               const std::vector<float>& moistureHeuristics);
               
    BiomeId getResult(uint32_t cellIdx) const;

private:
    float calculateEntropy(uint32_t cellIdx) const;
    bool collapseNext();
    bool propagate(uint32_t cellIdx);
    
    // Helper to calculate heuristic weight
    float getHeuristicWeight(uint32_t cellIdx, BiomeId biome) const;

    const VoronoiGraph& graph_;
    const BiomeConstraintGraph& constraints_;
    uint32_t seed_;
    std::mt19937 rng_;
    
    WFCState state_;
    std::stack<WFCState> history_;
    
    std::vector<float> tempHeuristics_;
    std::vector<float> moistHeuristics_;
};

} // namespace terrain

#endif // TERRAIN_WFC_H
