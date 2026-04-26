#include "voronoi.h"
#include "util.h"
#include <cmath>
#include <algorithm>

namespace terrain {

VoronoiGraph::VoronoiGraph(int width, int depth, float cellSize) {
    generateCells(width, depth, cellSize);
    computeNeighbors(gridW_, gridH_);
    mapGridPoints(width, depth);
}

void VoronoiGraph::generateCells(int width, int depth, float cellSize) {
    gridW_ = static_cast<int>(std::ceil(static_cast<float>(width) / cellSize));
    gridH_ = static_cast<int>(std::ceil(static_cast<float>(depth) / cellSize));
    
    cells_.reserve(static_cast<size_t>(gridW_) * static_cast<size_t>(gridH_));
    
    for (int j = 0; j < gridH_; ++j) {
        for (int i = 0; i < gridW_; ++i) {
            size_t idx = static_cast<size_t>(j * gridW_ + i);
            
            // Jittered grid point
            float offsetX = hashJitter(idx, 1337u);
            float offsetZ = hashJitter(idx, 7331u);
            
            VoronoiCell cell;
            cell.centerX = (static_cast<float>(i) + offsetX) * cellSize;
            cell.centerZ = (static_cast<float>(j) + offsetZ) * cellSize;
            cells_.push_back(cell);
        }
    }
}

void VoronoiGraph::computeNeighbors(int gridW, int gridH) {
    for (int j = 0; j < gridH; ++j) {
        for (int i = 0; i < gridW; ++i) {
            uint32_t idx = static_cast<uint32_t>(j * gridW + i);
            
            // Check 8 neighbors in the grid
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

void VoronoiGraph::mapGridPoints(int width, int depth) {
    gridToCellMap_.resize(static_cast<size_t>(width) * static_cast<size_t>(depth));
    
    // For each grid point, find the nearest cell
    // Since we know the cell size and grid structure, we only check nearby cells
    float cellSizeX = cells_.empty() ? 1.0f : cells_[1].centerX - cells_[0].centerX; // approximate
    // Use the same cellSize as constructor logic
    float approxCellSize = cells_.empty() ? 1.0f : std::sqrt(std::abs(cells_[0].centerX - cells_[gridW_ < 2 ? 0 : 1].centerX)); 
    // Actually, let's just use the grid coordinates directly
    
    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            float fx = static_cast<float>(x);
            float fz = static_cast<float>(z);
            
            // Find which grid square this point is in
            // This is a rough estimate to start the search
            // We'll check a small neighborhood around it
            
            float minDistSq = 1e18f;
            uint32_t bestCell = 0;
            
            // The point (x, z) is roughly in grid cell (x/cellSize, z/cellSize)
            // But we don't have the original cellSize here directly in the same way.
            // Let's just do a simple search for now, optimized later if needed.
            // Actually, we can use the centers to guide us.
            
            // Since it's a jittered grid, we check the cell it's "in" and its neighbors
            // For now, a brute force check of a local window is safe.
            // We'll just search all cells for now to be correct, then optimize.
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
