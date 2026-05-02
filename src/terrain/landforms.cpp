// for removal 
#include "landforms.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace terrain {

namespace {

LandformId classifyLandform(float elevationNorm, float temperature, float slope, float signal, float plateauWeight) {
    if (elevationNorm < 0.22f && slope < 0.09f && signal < 0.30f) {
        return LandformId::Lowland;
    }
    if (elevationNorm < 0.46f && slope < 0.14f && signal < 0.42f) {
        return LandformId::Valley;
    }
    if (plateauWeight > 0.30f && slope < 0.18f && elevationNorm > 0.20f && elevationNorm < 0.85f) {
        return LandformId::Plateau;
    }
    if (elevationNorm > 0.88f && temperature < 0.32f) {
        return LandformId::Snowcap;
    }
    if (elevationNorm > 0.74f && (temperature < 0.42f || signal > 0.74f)) {
        return LandformId::Alpine;
    }
    if (signal > 0.67f || (elevationNorm > 0.60f && slope > 0.12f)) {
        return LandformId::Mountain;
    }
    if (signal > 0.42f || (elevationNorm > 0.42f && slope > 0.07f)) {
        return LandformId::Foothill;
    }
    return LandformId::Plain;
}

int maxAllowedLevel(float elevationNorm, float temperature, float signal, float plateauWeight) {
    int maxLevel = static_cast<int>(LandformId::Plain);
    if (elevationNorm < 0.46f && signal < 0.42f) {
        maxLevel = static_cast<int>(LandformId::Valley);
    }
    if (plateauWeight > 0.25f && elevationNorm > 0.15f && elevationNorm < 0.88f) {
        maxLevel = static_cast<int>(LandformId::Plateau);
    }
    if (signal > 0.40f || elevationNorm > 0.38f) {
        maxLevel = static_cast<int>(LandformId::Foothill);
    }
    if (signal > 0.63f || elevationNorm > 0.58f) {
        maxLevel = static_cast<int>(LandformId::Mountain);
    }
    if (elevationNorm > 0.72f && (temperature < 0.45f || signal > 0.72f)) {
        maxLevel = static_cast<int>(LandformId::Alpine);
    }
    if (elevationNorm > 0.86f && temperature < 0.34f) {
        maxLevel = static_cast<int>(LandformId::Snowcap);
    }
    return maxLevel;
}

void blurScalarField(const std::vector<float>& src, std::vector<float>& dst, int width, int depth) {
    dst.resize(src.size());
    for (int z = 0; z < depth; ++z) {
        const int z0 = std::max(0, z - 1);
        const int z1 = std::min(depth - 1, z + 1);
        for (int x = 0; x < width; ++x) {
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(width - 1, x + 1);
            float sum = 0.0f;
            float weight = 0.0f;
            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const float sampleWeight = (nx == x && nz == z) ? 2.0f : 1.0f;
                    sum += src[fieldIndex(nx, nz, width)] * sampleWeight;
                    weight += sampleWeight;
                }
            }
            dst[fieldIndex(x, z, width)] = sum / std::max(0.0001f, weight);
        }
    }
}

void aggressiveBlurScalarField(
    const std::vector<float>& src,
    std::vector<float>& dst,
    int width,
    int depth,
    int radius) {
    dst.resize(src.size());
    for (int z = 0; z < depth; ++z) {
        const int z0 = std::max(0, z - radius);
        const int z1 = std::min(depth - 1, z + radius);
        for (int x = 0; x < width; ++x) {
            const int x0 = std::max(0, x - radius);
            const int x1 = std::min(width - 1, x + radius);
            float sum = 0.0f;
            float weight = 0.0f;
            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const int manhattan = std::abs(nx - x) + std::abs(nz - z);
                    const float sampleWeight = std::max(1.0f, static_cast<float>((radius * 2 + 2) - manhattan));
                    sum += src[fieldIndex(nx, nz, width)] * sampleWeight;
                    weight += sampleWeight;
                }
            }
            dst[fieldIndex(x, z, width)] = sum / std::max(0.0001f, weight);
        }
    }
}

void applyAggressiveLandformMajority(std::vector<uint8_t>& levels, const TerrainFields& fields, int passes) {
    constexpr size_t kCount = static_cast<size_t>(LandformId::Count);
    for (int pass = 0; pass < passes; ++pass) {
        std::vector<uint8_t> next = levels;
        for (int z = 0; z < fields.depth; ++z) {
            const int z0 = std::max(0, z - 2);
            const int z1 = std::min(fields.depth - 1, z + 2);
            for (int x = 0; x < fields.width; ++x) {
                const int x0 = std::max(0, x - 2);
                const int x1 = std::min(fields.width - 1, x + 2);
                const size_t idx = fieldIndex(x, z, fields.width);
                std::array<int, kCount> counts{};
                counts.fill(0);
                for (int nz = z0; nz <= z1; ++nz) {
                    for (int nx = x0; nx <= x1; ++nx) {
                        ++counts[levels[fieldIndex(nx, nz, fields.width)]];
                    }
                }
                size_t majority = static_cast<size_t>(levels[idx]);
                int best = counts[majority];
                for (size_t i = 0; i < counts.size(); ++i) {
                    if (counts[i] > best) {
                        best = counts[i];
                        majority = i;
                    }
                }
                if (best >= 12) {
                    next[idx] = static_cast<uint8_t>(majority);
                }
            }
        }
        levels.swap(next);
    }
}

void removeSmallLandformIslands(std::vector<uint8_t>& levels, const TerrainFields& fields) {
    const size_t total = levels.size();
    if (total == 0) {
        return;
    }

    constexpr int kMinComponentSize = 96;
    constexpr size_t kCount = static_cast<size_t>(LandformId::Count);
    std::vector<uint8_t> visited(total, 0);
    std::vector<uint8_t> mark(total, 0);
    std::queue<size_t> q;
    std::vector<size_t> component;
    component.reserve(256);

    for (size_t start = 0; start < total; ++start) {
        if (visited[start] != 0) {
            continue;
        }

        const uint8_t label = levels[start];
        if (label >= static_cast<uint8_t>(LandformId::Mountain)) {
            visited[start] = 1;
            continue;
        }

        q.push(start);
        visited[start] = 1;
        component.clear();
        component.push_back(start);

        while (!q.empty()) {
            const size_t idx = q.front();
            q.pop();
            const int x = static_cast<int>(idx % static_cast<size_t>(fields.width));
            const int z = static_cast<int>(idx / static_cast<size_t>(fields.width));
            const int z0 = std::max(0, z - 1);
            const int z1 = std::min(fields.depth - 1, z + 1);
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(fields.width - 1, x + 1);
            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const size_t nidx = fieldIndex(nx, nz, fields.width);
                    if (visited[nidx] != 0) {
                        continue;
                    }
                    visited[nidx] = 1;
                    if (levels[nidx] == label) {
                        q.push(nidx);
                        component.push_back(nidx);
                    }
                }
            }
        }

        if (static_cast<int>(component.size()) >= kMinComponentSize) {
            continue;
        }

        std::array<int, kCount> neighborCounts{};
        neighborCounts.fill(0);
        for (size_t idx : component) {
            mark[idx] = 1;
        }
        for (size_t idx : component) {
            const int x = static_cast<int>(idx % static_cast<size_t>(fields.width));
            const int z = static_cast<int>(idx / static_cast<size_t>(fields.width));
            const int z0 = std::max(0, z - 1);
            const int z1 = std::min(fields.depth - 1, z + 1);
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(fields.width - 1, x + 1);
            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const size_t nidx = fieldIndex(nx, nz, fields.width);
                    if (mark[nidx] != 0) {
                        continue;
                    }
                    ++neighborCounts[levels[nidx]];
                }
            }
        }
        size_t replacement = static_cast<size_t>(LandformId::Plain);
        int best = -1;
        for (size_t i = 0; i < neighborCounts.size(); ++i) {
            if (neighborCounts[i] > best) {
                best = neighborCounts[i];
                replacement = i;
            }
        }
        for (size_t idx : component) {
            levels[idx] = static_cast<uint8_t>(replacement);
            mark[idx] = 0;
        }
    }
}

} // namespace

} // namespace terrain
