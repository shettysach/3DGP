#pragma once
#include <vector>
#include <cstdint>

namespace terrain {

enum class RegionType {
    Mountain,
    Plains,
    River,
    Desert,
    Snow
};

struct Region {
    int id;
    float cx, cz;
    RegionType type;
    std::vector<int> neighbors;
};

struct AdjacencyConstraint {
    RegionType source;
    RegionType target;
    float weight;
};

class TerrainLayout {
public:
    void generate(int width, int depth, float scale, uint32_t seed);

    const Region& getRegionForPoint(float x, float z) const;

private:
    std::vector<Region> regions_;
    std::vector<AdjacencyConstraint> constraints_;

    float evaluateScore(const Region& region, RegionType candidate) const;
};

}