#include "layout.h"
#include <random>
#include <cmath>
#include <limits>
#include <iostream>

namespace terrain {

void TerrainLayout::generate(int width, int depth, float scale, uint32_t seed)
{
    regions_.clear();

    const int gridSize = 16;
    int id = 0;

    // --- Create regions ---
    for (int z = 0; z < depth; z += gridSize) {
        for (int x = 0; x < width; x += gridSize) {
            Region r;
            r.id = id++;
            r.cx = x * scale;
            r.cz = z * scale;
            r.type = RegionType::Plains;
            regions_.push_back(r);
        }
    }
    
    // constraints
    constraints_ = {
    // --- Self coherence (balanced) ---
    {RegionType::Mountain, RegionType::Mountain, 1.4f},
    {RegionType::Plains,   RegionType::Plains,   1.4f},
    {RegionType::River,    RegionType::River,   0.6f},

    // --- Natural adjacency ---
    {RegionType::Mountain, RegionType::Plains, 1.0f},
    {RegionType::Plains,   RegionType::Mountain, 1.0f},

    {RegionType::Plains,   RegionType::River, 1.2f},
    {RegionType::River,    RegionType::Plains, 1.2f},

    {RegionType::Mountain, RegionType::River, 0.3f},
    {RegionType::River,    RegionType::Mountain, 0.3f},

    // --- Avoid bad combos ---
    {RegionType::Desert, RegionType::River, -2.0f},
    {RegionType::Snow,   RegionType::River, -1.0f},
    {RegionType::Snow,   RegionType::Mountain, 1.0f}
};

    // --- Build neighbors ---
    float neighborDist = gridSize * scale * 1.5f;

    for (auto& a : regions_) {
        for (auto& b : regions_) {
            if (a.id == b.id) continue;

            float dx = a.cx - b.cx;
            float dz = a.cz - b.cz;
            float d2 = dx*dx + dz*dz;

            if (d2 < neighborDist * neighborDist) {
                a.neighbors.push_back(b.id);
            }
        }
    }

    // --- Initial random assignment ---
    std::vector<RegionType> allTypes = {
        RegionType::Mountain,
        RegionType::Plains,
        RegionType::River,
        RegionType::Desert,
        RegionType::Snow
    };

    std::mt19937 rng(seed);
    std::uniform_int_distribution<> dist(0, allTypes.size() - 1);

    for (auto& r : regions_) {
        r.type = allTypes[dist(rng)];
    }

    int riverCount = regions_.size() / 10; // adjust density

    for (int i = 0; i < riverCount; ++i)
    {
        int idx = rng() % regions_.size();
        regions_[idx].type = RegionType::River;
    }
    // --- Constraint solver ---
    for (int iter = 0; iter < 8; ++iter) {

    for (auto& r : regions_) {

        float bestScore = -std::numeric_limits<float>::max();
        RegionType bestType = r.type;

        for (auto candidate : allTypes) {

            float score = evaluateScore(r, candidate);

            // --- INERTIA (neutral, no biome bias) ---
            if (candidate == r.type) {
                score += 1.0f;
            }

            // --- RANDOMNESS (neutral symmetry breaking) ---
            float noise = (rng() % 1000) / 1000.0f;
            score += noise * 0.3f;

            if (score > bestScore) {
                bestScore = score;
                bestType = candidate;
            }
        }

        // --- SOFT UPDATE (neutral stability) ---
        if (bestType != r.type) {
            float changeProb = 0.5f; // no bias, just stability
            float roll = (rng() % 1000) / 1000.0f;

            if (roll < changeProb) {
                r.type = bestType;
            }
        }
    }
}

}

float TerrainLayout::evaluateScore(const Region& region, RegionType candidate) const
{
    float score = 0.0f;

    for (int nid : region.neighbors) {
        const Region& n = regions_[nid];

        for (const auto& c : constraints_) {
            if (c.source == candidate && c.target == n.type) {
                score += c.weight;
            }
        }
    }

    return score;
}

const Region& TerrainLayout::getRegionForPoint(float x, float z) const
{
    const Region* best = &regions_[0];
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& r : regions_) {
        float dx = x - r.cx;
        float dz = z - r.cz;
        float d = dx*dx + dz*dz;

        if (d < bestDist) {
            bestDist = d;
            best = &r;
        }
    }

    return *best;
}

}