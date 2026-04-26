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

struct RiverPoint {
    float x = 0.0f;
    float z = 0.0f;
    float strength = 0.0f;
};

float neighborStepCost(int dx, int dz) {
    return (dx != 0 && dz != 0) ? 1.41421356f : 1.0f;
}

void refineDownstreamDirections(
    const std::vector<float>& heights,
    const std::vector<float>& filledHeights,
    int width,
    int depth,
    const std::vector<int>& distanceToEdge,
    uint32_t seed,
    std::vector<int>& downstream) {
    constexpr float kDownhillEpsilon = 0.0001f;

    for (int z = 0; z < depth; ++z) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = fieldIndex(x, z, width);
            const int fallback = downstream[idx];
            if (fallback < 0) {
                continue;
            }

            const float currentFilled = filledHeights[idx];
            const float currentHeight = heights[idx];
            int bestDownstream = fallback;
            float bestScore = -1.0e9f;

            const int z0 = std::max(0, z - 1);
            const int z1 = std::min(depth - 1, z + 1);
            const int x0 = std::max(0, x - 1);
            const int x1 = std::min(width - 1, x + 1);

            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    if (nx == x && nz == z) {
                        continue;
                    }

                    const size_t nidx = fieldIndex(nx, nz, width);
                    const float filledDrop = currentFilled - filledHeights[nidx];
                    const int edgeProgress = distanceToEdge[idx] - distanceToEdge[nidx];
                    if (filledDrop <= kDownhillEpsilon && edgeProgress <= 0) {
                        continue;
                    }

                    const float rawDrop = currentHeight - heights[nidx];
                    const float stepCost = neighborStepCost(nx - x, nz - z);

                    float score = -1.0e9f;
                    if (filledDrop > kDownhillEpsilon) {
                        score = filledDrop / stepCost;
                        score += std::max(rawDrop, 0.0f) * 0.18f / stepCost;
                    } else {
                        score = static_cast<float>(edgeProgress) * 0.06f;
                        score += rawDrop * 0.12f / stepCost;
                    }

                    score += hashJitter(idx ^ (nidx * 0x9e3779b9u), seed) * 0.0005f;
                    if (score <= bestScore) {
                        continue;
                    }

                    bestScore = score;
                    bestDownstream = static_cast<int>(nidx);
                }
            }

            downstream[idx] = bestDownstream;
        }
    }
}

std::vector<RiverPoint> smoothRiverPath(std::vector<RiverPoint> path, int iterations) {
    if (path.size() < 3 || iterations <= 0) {
        return path;
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const size_t currentSize = path.size();
        std::vector<RiverPoint> smoothed;
        smoothed.reserve(currentSize * 2);
        smoothed.push_back(path.front());

        for (size_t i = 0; i + 1 < currentSize; ++i) {
            const RiverPoint& a = path[i];
            const RiverPoint& b = path[i + 1];
            smoothed.push_back({
                lerp(a.x, b.x, 0.25f),
                lerp(a.z, b.z, 0.25f),
                lerp(a.strength, b.strength, 0.25f),
            });
            smoothed.push_back({
                lerp(a.x, b.x, 0.75f),
                lerp(a.z, b.z, 0.75f),
                lerp(a.strength, b.strength, 0.75f),
            });
        }

        smoothed.push_back(path.back());
        path.swap(smoothed);
    }

    return path;
}

std::vector<RiverPoint> applyRiverMeander(
    std::vector<RiverPoint> path,
    int width,
    int depth,
    uint32_t seed,
    size_t sourceIdx) {
    const size_t pathSize = path.size();
    if (pathSize < 5) {
        return path;
    }

    std::vector<float> cumulativeLength(pathSize, 0.0f);
    for (size_t i = 1; i < pathSize; ++i) {
        const float dx = path[i].x - path[i - 1].x;
        const float dz = path[i].z - path[i - 1].z;
        cumulativeLength[i] = cumulativeLength[i - 1] + std::sqrt(dx * dx + dz * dz);
    }

    const float totalLength = cumulativeLength.back();
    if (totalLength <= 0.001f) {
        return path;
    }

    const float phaseA = hashJitter(sourceIdx, seed) * 6.28318531f;
    const float phaseB = hashJitter(sourceIdx ^ 0x9e3779b9u, seed + 17u) * 6.28318531f;
    const float cyclesA = std::max(1.2f, totalLength / 26.0f);
    const float cyclesB = std::max(2.0f, totalLength / 14.0f);
    const float maxOffset = 0.85f;

    for (size_t i = 1; i + 1 < pathSize; ++i) {
        const float tangentX = path[i + 1].x - path[i - 1].x;
        const float tangentZ = path[i + 1].z - path[i - 1].z;
        const float tangentLength = std::sqrt(tangentX * tangentX + tangentZ * tangentZ);
        if (tangentLength <= 0.0001f) {
            continue;
        }

        const float progress = cumulativeLength[i] / totalLength;
        const float envelope = std::pow(std::sin(progress * 3.14159265f), 0.85f);
        const float meanderStrength = (0.35f + 0.65f * path[i].strength) * envelope;
        const float wave =
            std::sin(progress * cyclesA * 6.28318531f + phaseA) * 0.65f +
            std::sin(progress * cyclesB * 6.28318531f + phaseB) * 0.35f;
        const float offset = std::clamp(wave * meanderStrength * maxOffset, -maxOffset, maxOffset);

        const float normalX = -tangentZ / tangentLength;
        const float normalZ = tangentX / tangentLength;
        path[i].x = std::clamp(path[i].x + normalX * offset, 0.35f, static_cast<float>(width - 1) - 0.35f);
        path[i].z = std::clamp(path[i].z + normalZ * offset, 0.35f, static_cast<float>(depth - 1) - 0.35f);
    }

    return path;
}

float distanceToSegment(float px, float pz, const RiverPoint& a, const RiverPoint& b, float& outT) {
    const float vx = b.x - a.x;
    const float vz = b.z - a.z;
    const float lengthSq = vx * vx + vz * vz;
    if (lengthSq <= 0.000001f) {
        outT = 0.0f;
        const float dx = px - a.x;
        const float dz = pz - a.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    const float wx = px - a.x;
    const float wz = pz - a.z;
    outT = std::clamp((wx * vx + wz * vz) / lengthSq, 0.0f, 1.0f);
    const float closestX = a.x + vx * outT;
    const float closestZ = a.z + vz * outT;
    const float dx = px - closestX;
    const float dz = pz - closestZ;
    return std::sqrt(dx * dx + dz * dz);
}

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

    refineDownstreamDirections(
        heights,
        filledHeights,
        width,
        depth,
        distanceToEdge,
        seed,
        downstream);

    const float minHeight = *std::min_element(heights.begin(), heights.end());
    const float maxHeight = *std::max_element(heights.begin(), heights.end());
    const float invHeightRange = 1.0f / std::max(0.0001f, maxHeight - minHeight);

    std::vector<size_t> order(cellCount);
    std::iota(order.begin(), order.end(), static_cast<size_t>(0));
    std::sort(order.begin(), order.end(), [&filledHeights, &distanceToEdge](size_t a, size_t b) {
        if (filledHeights[a] != filledHeights[b]) {
            return filledHeights[a] > filledHeights[b];
        }
        return distanceToEdge[a] > distanceToEdge[b];
    });

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

            const int cx = static_cast<int>(candidate.index % width);
            const int cz = static_cast<int>(candidate.index / width);

            bool tooClose = false;
            if (separationSq > 0) {
                for (size_t selected : selectedSources) {
                    const int sx = static_cast<int>(selected % width);
                    const int sz = static_cast<int>(selected / width);
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

    std::vector<float> riverStrengths(cellCount, 0.0f);
    for (size_t sourceIdx : selectedSources) {
        int current = static_cast<int>(sourceIdx);
        int guard = 0;
        while (current >= 0 && guard < width * depth) {
            const size_t cidx = static_cast<size_t>(current);
            activeRiver[cidx] = true;
            riverStrengths[cidx] = smoothstep(settings.sourceAccumulation, settings.mainAccumulation, accumulation[cidx]);

            const int down = downstream[cidx];
            if (down < 0 || down == current) {
                break;
            }

            current = down;
            ++guard;
        }
    }

    std::vector<float> carveDepths(cellCount, 0.0f);
    std::vector<unsigned char> pathCoverage(cellCount, 0u);

    for (size_t sourceIdx : selectedSources) {
        std::vector<RiverPoint> path;
        path.reserve(static_cast<size_t>(distanceToEdge[sourceIdx] + 1));

        int current = static_cast<int>(sourceIdx);
        int guard = 0;
        while (current >= 0 && guard < width * depth) {
            const size_t cidx = static_cast<size_t>(current);
            const int cx = current % width;
            const int cz = current / width;
            path.push_back({
                static_cast<float>(cx),
                static_cast<float>(cz),
                riverStrengths[cidx],
            });

            if (pathCoverage[cidx]) {
                break;
            }
            pathCoverage[cidx] = 1u;

            const int down = downstream[cidx];
            if (down < 0 || down == current || !activeRiver[static_cast<size_t>(down)]) {
                break;
            }

            current = down;
            ++guard;
        }

        if (path.size() < 2) {
            continue;
        }

        // Smooth the centerline before carving so the river follows a natural-looking course
        // even though the drainage network itself is still tracked on the terrain grid.
        path = smoothRiverPath(std::move(path), 2);
        path = applyRiverMeander(std::move(path), width, depth, seed, sourceIdx);
        path = smoothRiverPath(std::move(path), 1);

        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const RiverPoint& a = path[i];
            const RiverPoint& b = path[i + 1];
            const float radiusA = std::max(0.65f, lerp(0.55f, static_cast<float>(settings.maxHalfWidth) + 0.35f, a.strength));
            const float radiusB = std::max(0.65f, lerp(0.55f, static_cast<float>(settings.maxHalfWidth) + 0.35f, b.strength));
            const float maxRadius = std::max(radiusA, radiusB);

            const int x0 = std::max(0, static_cast<int>(std::floor(std::min(a.x, b.x) - maxRadius - 1.0f)));
            const int x1 = std::min(width - 1, static_cast<int>(std::ceil(std::max(a.x, b.x) + maxRadius + 1.0f)));
            const int z0 = std::max(0, static_cast<int>(std::floor(std::min(a.z, b.z) - maxRadius - 1.0f)));
            const int z1 = std::min(depth - 1, static_cast<int>(std::ceil(std::max(a.z, b.z) + maxRadius + 1.0f)));

            for (int nz = z0; nz <= z1; ++nz) {
                for (int nx = x0; nx <= x1; ++nx) {
                    float segmentT = 0.0f;
                    const float dist = distanceToSegment(static_cast<float>(nx), static_cast<float>(nz), a, b, segmentT);
                    const float radius = lerp(radiusA, radiusB, segmentT);
                    if (dist > radius) {
                        continue;
                    }

                    const float strength = lerp(a.strength, b.strength, segmentT);
                    const float bank = 1.0f - smoothstep(0.0f, radius, dist);
                    const float falloff = std::pow(bank, settings.bankFalloff);
                    const float carveDepth =
                        verticalScale * lerp(settings.baseCarveFraction, settings.maxCarveFraction, strength);
                    const size_t nidx = fieldIndex(nx, nz, width);

                    carveDepths[nidx] = std::max(carveDepths[nidx], carveDepth * falloff);
                    out.riverWeights[nidx] = std::max(out.riverWeights[nidx], bank * (0.45f + 0.55f * strength));
                }
            }
        }
    }

    for (size_t idx = 0; idx < cellCount; ++idx) {
        if (carveDepths[idx] <= 0.0f) {
            continue;
        }

        out.carvedHeights[idx] -= carveDepths[idx];
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
