#include "wfc.h"
#include <algorithm>
#include <numeric>
#include <iostream>

namespace terrain {

/**
 * Constructs the adjacency rules for all biomes.
 * This defines which biomes are allowed to be next to each other in the terrain.
 */
BiomeConstraintGraph::BiomeConstraintGraph() {
    std::vector<BiomeId> deserts = {BiomeId::DesertPlain, BiomeId::DesertPlateau};
    std::vector<BiomeId> steppes = {BiomeId::SteppePlain, BiomeId::SteppeFoothill, BiomeId::SteppePlateau};
    std::vector<BiomeId> grasslands = {BiomeId::GrasslandPlain, BiomeId::GrasslandFoothill, BiomeId::GrasslandPlateau, BiomeId::MarshLowland};
    std::vector<BiomeId> forests = {BiomeId::ForestPlain, BiomeId::ForestFoothill, BiomeId::ForestPlateau};
    std::vector<BiomeId> taigas = {BiomeId::TaigaPlain, BiomeId::TaigaFoothill, BiomeId::TaigaPlateau};
    std::vector<BiomeId> tundras = {BiomeId::TundraPlain, BiomeId::TundraPlateau};
    std::vector<BiomeId> alpines = {BiomeId::RockyAlpine, BiomeId::Alpine, BiomeId::Snow};

    auto connectGroups = [&](const std::vector<BiomeId>& g1, const std::vector<BiomeId>& g2) {
        for (BiomeId a : g1) {
            for (BiomeId b : g2) addRule(a, b);
        }
    };

    connectGroups(deserts, deserts);
    connectGroups(deserts, steppes);
    connectGroups(deserts, grasslands);

    connectGroups(steppes, steppes);
    connectGroups(steppes, grasslands);
    connectGroups(steppes, forests);

    connectGroups(grasslands, grasslands);
    connectGroups(grasslands, forests);
    connectGroups(grasslands, taigas);

    connectGroups(forests, forests);
    connectGroups(forests, taigas);

    connectGroups(taigas, taigas);
    connectGroups(taigas, tundras);
    connectGroups(taigas, alpines);

    connectGroups(tundras, tundras);
    connectGroups(tundras, alpines);

    connectGroups(alpines, alpines);
}

/**
 * Registers a bi-directional adjacency rule between two biomes.
 */
void BiomeConstraintGraph::addRule(BiomeId a, BiomeId b) {
    adjacencyRules_[a].push_back(b);
    if (a != b) {
        adjacencyRules_[b].push_back(a);
    }
}

/**
 * Returns true if biome 'b' is allowed to be placed adjacent to biome 'a'.
 */
bool BiomeConstraintGraph::isCompatible(BiomeId a, BiomeId b) const {
    auto it = adjacencyRules_.find(a);
    if (it == adjacencyRules_.end()) return false;
    for (auto other : it->second) {
        if (other == b) return true;
    }
    return false;
}

/**
 * Returns a list of all biomes compatible with the given biome.
 */
const std::vector<BiomeId>& BiomeConstraintGraph::getCompatible(BiomeId a) const {
    static const std::vector<BiomeId> empty;
    auto it = adjacencyRules_.find(a);
    return (it != adjacencyRules_.end()) ? it->second : empty;
}

/**
 * Prepares the solver state, marking all biomes as possible for all cells initially.
 */
WFCBiomeSolver::WFCBiomeSolver(const VoronoiGraph& graph, const BiomeConstraintGraph& constraints, uint32_t seed)
    : graph_(graph), constraints_(constraints), rng_(seed) {
    
    size_t cellCount = graph_.cells().size();
    size_t biomeCount = static_cast<size_t>(BiomeId::Count);
    
    state_.possibilities.assign(cellCount * biomeCount, true);
    state_.collapsedBiomes.assign(cellCount, -1);
}

/**
 * Runs the Wave Function Collapse algorithm.
 * Uses a combination of entropy-based selection, constraint propagation, and backtracking.
 */
bool WFCBiomeSolver::solve(const std::vector<float>& temperatureHeuristics, 
                           const std::vector<float>& moistureHeuristics) {
    
    tempHeuristics_ = temperatureHeuristics;
    moistHeuristics_ = moistureHeuristics;
    
    int maxIterations = state_.collapsedBiomes.size() * 50;
    int iterations = 0;
    
    while (iterations++ < maxIterations) {
        if (!collapseNext()) {
            bool finished = true;
            for (auto b : state_.collapsedBiomes) {
                if (b == -1) { finished = false; break; }
            }
            if (finished) return true;
            
            if (history_.empty()) {
                std::cout << "[WFC] Fatal contradiction. No history left to backtrack!" << std::endl;
                return false;
            }
            
            state_ = history_.top();
            history_.pop();
            
            if (state_.lastChoiceCell != -1 && state_.lastChoiceBiome != -1) {
                int biomeCount = static_cast<int>(BiomeId::Count);
                state_.possibilities[state_.lastChoiceCell * biomeCount + state_.lastChoiceBiome] = false;
            }
        }
    }
    std::cout << "[WFC] Failed: Exceeded maximum iterations (" << maxIterations << ")." << std::endl;
    return false;
}

/**
 * Calculates the Shannon entropy (remaining possibilities) for a cell.
 * Cells with lower entropy are prioritized for collapsing to minimize contradictions.
 */
float WFCBiomeSolver::calculateEntropy(uint32_t cellIdx) const {
    if (state_.collapsedBiomes[cellIdx] != -1) return 1e10f;
    
    int count = 0;
    int biomeCount = static_cast<int>(BiomeId::Count);
    for (int i = 0; i < biomeCount; ++i) {
        if (state_.possibilities[cellIdx * biomeCount + i]) count++;
    }
    
    if (count == 0) return -1.0f;
    
    std::uniform_real_distribution<float> dist(0.0f, 0.1f);
    return static_cast<float>(count) + dist(const_cast<std::mt19937&>(rng_));
}

/**
 * Finds the cell with the lowest entropy and collapses it to a single biome.
 * Biome selection is weighted by climate heuristics and neighbor continuity.
 */
bool WFCBiomeSolver::collapseNext() {
    uint32_t bestCell = 0;
    float minEntropy = 1e11f;
    bool found = false;
    
    for (uint32_t i = 0; i < state_.collapsedBiomes.size(); ++i) {
        if (state_.collapsedBiomes[i] != -1) continue;
        
        float entropy = calculateEntropy(i);
        if (entropy < 0) return false;
        if (entropy < minEntropy) {
            minEntropy = entropy;
            bestCell = i;
            found = true;
        }
    }
    
    if (!found) return false;
    
    std::vector<int> possible;
    std::vector<float> weights;
    float weightSum = 0.0f;
    
    int biomeCount = static_cast<int>(BiomeId::Count);
    
    auto getEcologyGroup = [](BiomeId b) -> int {
        switch(b) {
            case BiomeId::DesertPlain: case BiomeId::DesertPlateau: return 0;
            case BiomeId::SteppePlain: case BiomeId::SteppeFoothill: case BiomeId::SteppePlateau: return 1;
            case BiomeId::GrasslandPlain: case BiomeId::GrasslandFoothill: case BiomeId::GrasslandPlateau: return 2;
            case BiomeId::ForestPlain: case BiomeId::ForestFoothill: case BiomeId::ForestPlateau: return 3;
            case BiomeId::TaigaPlain: case BiomeId::TaigaFoothill: case BiomeId::TaigaPlateau: return 4;
            case BiomeId::TundraPlain: case BiomeId::TundraPlateau: return 5;
            case BiomeId::RockyAlpine: case BiomeId::Alpine: case BiomeId::Snow: return 6;
            case BiomeId::MarshLowland: return 7;
            default: return 8;
        }
    };
    
    std::vector<int> neighborEcologyCounts(9, 0);
    for (uint32_t neighbor : graph_.cells()[bestCell].neighborIndices) {
        if (state_.collapsedBiomes[neighbor] != -1) {
            int eco = getEcologyGroup(static_cast<BiomeId>(state_.collapsedBiomes[neighbor]));
            neighborEcologyCounts[eco]++;
        }
    }
    
    for (int i = 0; i < biomeCount; ++i) {
        if (state_.possibilities[bestCell * biomeCount + i]) {
            possible.push_back(i);
            
            float weight = getHeuristicWeight(bestCell, static_cast<BiomeId>(i));
            
            int myEco = getEcologyGroup(static_cast<BiomeId>(i));
            if (neighborEcologyCounts[myEco] > 0) {
                weight *= (1.0f + 100.0f * neighborEcologyCounts[myEco]);
            }

            weight = std::max(0.01f, weight);
            weights.push_back(weight);
            weightSum += weight;
        }
    }
    
    if (possible.empty()) return false;
    
    std::uniform_real_distribution<float> weightDist(0.0f, weightSum);
    float target = weightDist(rng_);
    int selected = possible.back();
    
    float currentSum = 0.0f;
    for (size_t i = 0; i < possible.size(); ++i) {
        currentSum += weights[i];
        if (currentSum >= target) {
            selected = possible[i];
            break;
        }
    }
    
    state_.lastChoiceCell = bestCell;
    state_.lastChoiceBiome = selected;
    history_.push(state_);
    
    state_.collapsedBiomes[bestCell] = static_cast<int8_t>(selected);
    for (int i = 0; i < biomeCount; ++i) {
        state_.possibilities[bestCell * biomeCount + i] = (i == selected);
    }
    
    return propagate(bestCell);
}

/**
 * Propagates constraints from a newly collapsed cell to its neighbors.
 * This removes biome possibilities from neighbors that are no longer compatible.
 */
bool WFCBiomeSolver::propagate(uint32_t cellIdx) {
    std::stack<uint32_t> dirty;
    dirty.push(cellIdx);
    
    while (!dirty.empty()) {
        uint32_t curr = dirty.top();
        dirty.pop();
        
        for (uint32_t neighbor : graph_.cells()[curr].neighborIndices) {
            bool changed = false;
            int biomeCount = static_cast<int>(BiomeId::Count);
            
            for (int nb = 0; nb < biomeCount; ++nb) {
                if (!state_.possibilities[neighbor * biomeCount + nb]) continue;
                
                bool compatible = false;
                for (int cb = 0; cb < biomeCount; ++cb) {
                    if (state_.possibilities[curr * biomeCount + cb] && constraints_.isCompatible(static_cast<BiomeId>(cb), static_cast<BiomeId>(nb))) {
                        compatible = true;
                        break;
                    }
                }
                
                if (!compatible) {
                    state_.possibilities[neighbor * biomeCount + nb] = false;
                    changed = true;
                }
            }
            
            if (changed) {
                int count = 0;
                for (int i = 0; i < biomeCount; ++i) {
                    if (state_.possibilities[neighbor * biomeCount + i]) count++;
                }
                if (count == 0) return false;
                
                dirty.push(neighbor);
            }
        }
    }
    
    return true;
}

/**
 * Returns the final biome selected for a specific Voronoi cell.
 */
BiomeId WFCBiomeSolver::getResult(uint32_t cellIdx) const {
    if (state_.collapsedBiomes[cellIdx] == -1) return BiomeId::GrasslandPlain;
    return static_cast<BiomeId>(state_.collapsedBiomes[cellIdx]);
}

/**
 * Calculates a weight for a biome based on the local temperature and moisture.
 * This guides the WFC solver to place biomes in appropriate climate zones.
 */
float WFCBiomeSolver::getHeuristicWeight(uint32_t cellIdx, BiomeId biome) const {
    float cellTemp = tempHeuristics_[cellIdx];
    float cellMoist = moistHeuristics_[cellIdx];
    
    float targetTemp = 0.5f;
    float targetMoist = 0.5f;
    
    switch(biome) {
        case BiomeId::Snow:
        case BiomeId::Alpine:
        case BiomeId::RockyAlpine:
            targetTemp = 0.0f; targetMoist = 0.5f; break;
        case BiomeId::TundraPlain:
        case BiomeId::TundraPlateau:
            targetTemp = 0.12f; targetMoist = 0.34f; break;
        case BiomeId::TaigaPlain:
        case BiomeId::TaigaFoothill:
        case BiomeId::TaigaPlateau:
            targetTemp = 0.28f; targetMoist = 0.58f; break;
        case BiomeId::ForestPlain:
        case BiomeId::ForestFoothill:
        case BiomeId::ForestPlateau:
            targetTemp = 0.56f; targetMoist = 0.72f; break;
        case BiomeId::GrasslandPlain:
        case BiomeId::GrasslandFoothill:
        case BiomeId::GrasslandPlateau:
            targetTemp = 0.54f; targetMoist = 0.48f; break;
        case BiomeId::SteppePlain:
        case BiomeId::SteppeFoothill:
        case BiomeId::SteppePlateau:
            targetTemp = 0.66f; targetMoist = 0.26f; break;
        case BiomeId::DesertPlain:
        case BiomeId::DesertPlateau:
            targetTemp = 0.82f; targetMoist = 0.10f; break;
        case BiomeId::MarshLowland:
            targetTemp = 0.44f; targetMoist = 0.88f; break;
        default: break;
    }
    
    float tDiff = cellTemp - targetTemp;
    float mDiff = cellMoist - targetMoist;
    float distSq = (tDiff * tDiff) + (mDiff * mDiff);
   
    return std::exp(-distSq * 15.0f);
}

} // namespace terrain
