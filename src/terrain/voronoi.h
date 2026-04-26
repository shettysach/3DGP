#ifndef TERRAIN_VORONOI_H
#define TERRAIN_VORONOI_H

#include <vector>
#include <cstdint>

namespace terrain {

struct VoronoiCell {
    float centerX = 0.0f;
    float centerZ = 0.0f;
    std::vector<uint32_t> neighborIndices;
    
    // Grid points that belong to this cell
    std::vector<size_t> gridIndices;
};

class VoronoiGraph {
public:
    VoronoiGraph(int width, int depth, float cellSize);

    const std::vector<VoronoiCell>& cells() const { return cells_; }
    
    // Maps each grid point to a cell index
    const std::vector<uint32_t>& gridToCellMap() const { return gridToCellMap_; }

private:
    void generateCells(int width, int depth, float cellSize);
    void computeNeighbors(int gridW, int gridH);
    void mapGridPoints(int width, int depth);

    std::vector<VoronoiCell> cells_;
    std::vector<uint32_t> gridToCellMap_;
    int gridW_ = 0;
    int gridH_ = 0;
};

} // namespace terrain

#endif // TERRAIN_VORONOI_H
