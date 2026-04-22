#include "rivers.h"
#include "fields.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>

namespace terrain {

namespace {

struct FloodNode {
    float elevation = 0.0f;
    int index = -1;
};

struct FloodNodeGreater {
    bool operator()(const FloodNode& a, const FloodNode& b) const {
        return a.elevation > b.elevation;
    }
};

struct SourceCandidate {
    size_t index = 0;
    float score = 0.0f;
};

void seedBoundaryCell(
    int x,
    int z,
    const std::vector<float>& heights,
    int width,
    std::vector<unsigned char>& visited,
    std::vector<float>& filledHeights,
    std::priority_queue<FloodNode, std::vector<FloodNode>, FloodNodeGreater>& flood) {
    const size_t idx = fieldIndex(x, z, width);
    if (visited[idx]) {
        return;
    }

    visited[idx] = 1u;
    filledHeights[idx] = heights[idx];
    flood.push({filledHeights[idx], static_cast<int>(idx)});
}

} // namespace

RiverPassResult runRiverPass(
    const std::vector<float>& heights,
    int width,
    int depth,
    float verticalScale,
    const RiverSettings& settings,
    uint32_t seed) {
    RiverPassResult out;
    out.carvedHeights = heights;
    out.riverWeights.assign(heights.size(), 0.0f);

    if (width < 2 || depth < 2 || heights.empty()) {
        return out;
    }

    const size_t cellCount = static_cast<size_t>(width) * static_cast<size_t>(depth);
    std::vector<int> downstream(cellCount, -1);
    std::vector<int> distanceToEdge(cellCount, 0);
    std::vector<float> filledHeights(cellCount, 0.0f);
    std::vector<float> accumulation(cellCount, 1.0f);
    std::vector<bool> activeRiver(cellCount, false);
    std::vector<unsigned char> visited(cellCount, 0u);

    std::priority_queue<FloodNode, std::vector<FloodNode>, FloodNodeGreater> flood;

    for (int x = 0; x < width; ++x) {
        seedBoundaryCell(x, 0, heights, width, visited, filledHeights, flood);
        seedBoundaryCell(x, depth - 1, heights, width, visited, filledHeights, flood);
    }
    for (int z = 0; z < depth; ++z) {
        seedBoundaryCell(0, z, heights, width, visited, filledHeights, flood);
        seedBoundaryCell(width - 1, z, heights, width, visited, filledHeights, flood);
    }

    while (!flood.empty()) {
        const FloodNode node = flood.top();
        flood.pop();

        const int cx = node.index % width;
        const int cz = node.index / width;

        const int z0 = std::max(0, cz - 1);
        const int z1 = std::min(depth - 1, cz + 1);
        const int x0 = std::max(0, cx - 1);
        const int x1 = std::min(width - 1, cx + 1);

        for (int nz = z0; nz <= z1; ++nz) {
            for (int nx = x0; nx <= x1; ++nx) {
                if (nx == cx && nz == cz) {
                    continue;
                }

                const size_t nidx = fieldIndex(nx, nz, width);
                if (visited[nidx]) {
                    continue;
                }

                visited[nidx] = 1u;
                filledHeights[nidx] = std::max(heights[nidx], node.elevation);
                downstream[nidx] = node.index;
                distanceToEdge[nidx] = distanceToEdge[static_cast<size_t>(node.index)] + 1;
                flood.push({filledHeights[nidx], static_cast<int>(nidx)});
            }
        }
    }

    const float minHeight = *std::min_element(heights.begin(), heights.end());
    const float maxHeight = *std::max_element(heights.begin(), heights.end());
    const float invHeightRange = 1.0f / std::max(0.0001f, maxHeight - minHeight);

    std::vector<size_t> order(cellCount);
    std::iota(order.begin(), order.end(), static_cast<size_t>(0));
    std::sort(order.begin(), order.end(), [&distanceToEdge](size_t a, size_t b) { return distanceToEdge[a] > distanceToEdge[b]; });

    for (size_t idx : order) {
        const int down = downstream[idx];
        if (down >= 0) {
            accumulation[static_cast<size_t>(down)] += accumulation[idx];
        }
    }

    std::vector<float> maxUpstreamAccumulation(cellCount, 0.0f);
    for (size_t idx = 0; idx < cellCount; ++idx) {
        const int down = downstream[idx];
        if (down >= 0) {
            maxUpstreamAccumulation[static_cast<size_t>(down)] =
                std::max(maxUpstreamAccumulation[static_cast<size_t>(down)], accumulation[idx]);
        }
    }

    const int minRiverLength = std::max(12, (width + depth) / 20);
    std::vector<SourceCandidate> sourceCandidates;
    sourceCandidates.reserve(cellCount / 16u);

    for (size_t idx : order) {
        const float hNorm = (heights[idx] - minHeight) * invHeightRange;
        if (hNorm < settings.minSourceHeight ||
            accumulation[idx] < settings.sourceAccumulation ||
            distanceToEdge[idx] < minRiverLength) {
            continue;
        }

        if (maxUpstreamAccumulation[idx] >= settings.sourceAccumulation) {
            continue;
        }

        const float lengthFactor = std::clamp(static_cast<float>(distanceToEdge[idx]) /
                                                  std::max(1.0f, static_cast<float>(width + depth)),
                                              0.0f,
                                              1.0f);
        const float accumulationFactor = std::log1p(accumulation[idx]);
        const float score =
            accumulationFactor * (0.35f + 0.65f * hNorm) +
            lengthFactor * 2.5f +
            hashJitter(idx, seed) * 0.003f;
        sourceCandidates.push_back({idx, score});
    }

    if (sourceCandidates.empty()) {
        return out;
    }

    std::sort(sourceCandidates.begin(), sourceCandidates.end(), [](const SourceCandidate& a, const SourceCandidate& b) { return a.score > b.score; });

    const int desiredSources = std::max(
        1,
        static_cast<int>(std::round(settings.sourceDensity * static_cast<float>(cellCount))));
    const int baseSeparation = std::max(0, settings.minSourceSeparation);
    std::vector<size_t> selectedSources;
    selectedSources.reserve(static_cast<size_t>(desiredSources));
    std::vector<unsigned char> selectedMask(cellCount, 0u);

    for (int pass = 0; pass < 3 && static_cast<int>(selectedSources.size()) < desiredSources; ++pass) {
        int separation = baseSeparation;
        if (pass == 1) {
            separation = baseSeparation / 2;
        } else if (pass == 2) {
            separation = 0;
        }

        const int separationSq = separation * separation;

        for (const SourceCandidate& candidate : sourceCandidates) {
            if (static_cast<int>(selectedSources.size()) >= desiredSources) {
                break;
            }

            if (selectedMask[candidate.index]) {
                continue;
            }

            const int cx = static_cast<int>(candidate.index % static_cast<size_t>(width));
            const int cz = static_cast<int>(candidate.index / static_cast<size_t>(width));

            bool tooClose = false;
            if (separationSq > 0) {
                for (size_t selected : selectedSources) {
                    const int sx = static_cast<int>(selected % static_cast<size_t>(width));
                    const int sz = static_cast<int>(selected / static_cast<size_t>(width));
                    const int dx = cx - sx;
                    const int dz = cz - sz;
                    if (dx * dx + dz * dz < separationSq) {
                        tooClose = true;
                        break;
                    }
                }
            }

            if (tooClose) {
                continue;
            }

            selectedSources.push_back(candidate.index);
            selectedMask[candidate.index] = 1u;
        }
    }

    if (selectedSources.empty()) {
        selectedSources.push_back(sourceCandidates.front().index);
    }

    for (size_t sourceIdx : selectedSources) {
        int current = static_cast<int>(sourceIdx);
        int guard = 0;
        while (current >= 0 && guard < width * depth) {
            const size_t cidx = static_cast<size_t>(current);
            activeRiver[cidx] = true;

            const int down = downstream[cidx];
            if (down < 0 || down == current) {
                break;
            }

            current = down;
            ++guard;
        }
    }

    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = fieldIndex(x, z, width);
            if (!activeRiver[idx]) {
                continue;
            }

            const float strength = smoothstep(settings.sourceAccumulation, settings.mainAccumulation, accumulation[idx]);
            const int halfWidth = std::max(1, 1 + static_cast<int>(std::round(strength * static_cast<float>(settings.maxHalfWidth))));
            const float carveDepth = verticalScale * lerp(settings.baseCarveFraction, settings.maxCarveFraction, strength);

            const int z0 = std::max(0, z - halfWidth);
            const int z1 = std::min(depth - 1, z + halfWidth);
            const int x0 = std::max(0, x - halfWidth);
            const int x1 = std::min(width - 1, x + halfWidth);

            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    const float dx = static_cast<float>(nx - x);
                    const float dz = static_cast<float>(nz - z);
                    const float dist = std::sqrt(dx * dx + dz * dz);
                    if (dist > static_cast<float>(halfWidth)) {
                        continue;
                    }

                    const float t = std::clamp(1.0f - dist / std::max(0.001f, static_cast<float>(halfWidth)), 0.0f, 1.0f);
                    const float falloff = std::pow(t, settings.bankFalloff);
                    const size_t nidx = fieldIndex(nx, nz, width);

                    out.carvedHeights[nidx] -= carveDepth * falloff;
                    out.riverWeights[nidx] = std::max(out.riverWeights[nidx], falloff * (0.45f + 0.55f * strength));
                }
            }
        }
    }

    std::vector<float> smoothed = out.carvedHeights;
    for (int z = 0; z < depth; ++z) {
        const int z0 = std::max(0, z - 1);
        const int z1 = std::min(depth - 1, z + 1);
        for (int x = 0; x < width; ++x) {
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(width - 1, x + 1);
            const size_t idx = fieldIndex(x, z, width);
            if (out.riverWeights[idx] <= 0.001f) {
                continue;
            }

            const float filtered = (out.carvedHeights[fieldIndex(x0, z0, width)] + 2.0f * out.carvedHeights[fieldIndex(x, z0, width)] + out.carvedHeights[fieldIndex(x1, z0, width)] +
                                    2.0f * out.carvedHeights[fieldIndex(x0, z, width)] + 4.0f * out.carvedHeights[idx] +
                                    2.0f * out.carvedHeights[fieldIndex(x1, z, width)] + out.carvedHeights[fieldIndex(x0, z1, width)] +
                                    2.0f * out.carvedHeights[fieldIndex(x, z1, width)] + out.carvedHeights[fieldIndex(x1, z1, width)]) /
                                   16.0f;

            const float blend = std::clamp(out.riverWeights[idx] * 0.45f, 0.0f, 0.45f);
            smoothed[idx] = lerp(out.carvedHeights[idx], filtered, blend);
        }
    }

    out.carvedHeights.swap(smoothed);
    return out;
}

} // namespace terrain
