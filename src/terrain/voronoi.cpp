#include "voronoi.h"
#include "util.h"
#include <cmath>
#include <algorithm>

namespace terrain {

/**
 * Initializes the Voronoi graph by generating cell centers, 
 * calculating adjacency, and mapping the grid pixels to territories.
 */
VoronoiGraph::VoronoiGraph(int width, int depth, float cellSize) {
    generateCells(width, depth, cellSize);
    computeNeighbors(gridW_, gridH_);
    mapGridPoints(width, depth);
}

/**
 * Creates a jittered grid of Voronoi centers. 
 * This ensures the territories look natural and irregular rather than perfectly square.
 */
void VoronoiGraph::generateCells(int width, int depth, float cellSize) {
    gridW_ = static_cast<int>(std::ceil(static_cast<float>(width) / cellSize));
    gridH_ = static_cast<int>(std::ceil(static_cast<float>(depth) / cellSize));
    
    cells_.reserve(static_cast<size_t>(gridW_) * static_cast<size_t>(gridH_));
    
    for (int j = 0; j < gridH_; ++j) {
        for (int i = 0; i < gridW_; ++i) {
            size_t idx = static_cast<size_t>(j * gridW_ + i);
            
            float offsetX = hashJitter(idx, 1337u);
            float offsetZ = hashJitter(idx, 7331u);
            
            VoronoiCell cell;
            cell.centerX = (static_cast<float>(i) + offsetX) * cellSize;
            cell.centerZ = (static_cast<float>(j) + offsetZ) * cellSize;
            cells_.push_back(cell);
        }
    }
}

/**
 * Connects each cell to its 8 immediate neighbors in the grid.
 * This adjacency information is used by the WFC solver to enforce biome rules.
 */
void VoronoiGraph::computeNeighbors(int gridW, int gridH) {
    for (int j = 0; j < gridH; ++j) {
        for (int i = 0; i < gridW; ++i) {
            uint32_t idx = static_cast<uint32_t>(j * gridW + i);
            
            for (int nj = j - 1; nj <= j + 1; ++nj) {
                for (int ni = i - 1; ni <= i + 1; ++ni) {
                    if (ni == i && nj == j) continue;
                    if (ni >= 0 && ni < gridW && nj >= 0 && nj < gridH) {
                        cells_[idx].neighborIndices.push_back(static_cast<uint32_t>(nj * gridW + ni));
                    }
                }
            }
        }
    }
}

/**
 * Assigns every pixel on the high-resolution terrain grid to its closest Voronoi cell.
 * This effectively "fills in" the territories so we can render the biomes later.
 */
void VoronoiGraph::mapGridPoints(int width, int depth) {
    gridToCellMap_.resize(static_cast<size_t>(width) * static_cast<size_t>(depth));
    
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            float fx = static_cast<float>(x);
            float fz = static_cast<float>(z);
            
            float minDistSq = 1e18f;
            uint32_t bestCell = 0;
            
            for (uint32_t cIdx = 0; cIdx < static_cast<uint32_t>(cells_.size()); ++cIdx) {
                float dx = fx - cells_[cIdx].centerX;
                float dz = fz - cells_[cIdx].centerZ;
                float distSq = dx * dx + dz * dz;
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    bestCell = cIdx;
                }
            }
            
            size_t gridIdx = static_cast<size_t>(z * width + x);
            gridToCellMap_[gridIdx] = bestCell;
            cells_[bestCell].gridIndices.push_back(gridIdx);
        }
    }
}

} // namespace terrain
