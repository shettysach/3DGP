#include "graph/graph_execute.h"
#include "../terrain/blending.h"
#include "../terrain/fields.h"
#include "../terrain/mountains.h"
#include "../terrain/plains.h"
#include "../terrain/plateaus.h"
#include "../terrain/util.h"
#include "../terrain/valleys.h"
#include "graph/types.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace graph {

using terrain::BlendResult;
using terrain::blendTerrain;
using terrain::computeMountain;
using terrain::computePlainsHeight;
using terrain::computePlateau;
using terrain::computeValley;
using terrain::fieldIndex;
using terrain::MountainResult;
using terrain::PlateauResult;
using terrain::smoothstep;
using terrain::ValleyResult;

namespace {

float islandFalloff(const terrain::TerrainSettings& settings, float worldX, float worldZ,
                    float centerX, float centerZ, float maxRadius) {
    if (!settings.islandFalloff)
        return 1.0f;
    const float dx = worldX - centerX;
    const float dz = worldZ - centerZ;
    const float radius = std::sqrt(dx * dx + dz * dz);
    const float scaled = std::max(0.0001f, maxRadius * settings.falloffRadius);
    float t = 1.0f - radius / scaled;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::pow(t, settings.falloffPower);
}

} // namespace

terrain::TerrainFields execute(const CompiledGraph& compiled,
                               const terrain::TerrainSettings& settings,
                               const terrain::NoiseContext& noiseContext) {

    const size_t N = compiled.nodes.size();
    const int w = settings.width;
    const int d = settings.depth;
    const float hScale = settings.horizontalScale;
    const size_t cellCount = static_cast<size_t>(w) * static_cast<size_t>(d);

    // Find TerrainSynthesis root
    size_t rootIdx = 0;
    for (size_t i = 0; i < N; ++i) {
        if (compiled.nodes[i].kind == NodeKind::TerrainSynthesis) {
            rootIdx = i;
            break;
        }
    }

    std::vector<std::vector<float>> nodeOutputs(N);
    std::vector<bool> visited(N, false);
    terrain::TerrainFields fields(w, d);

    const float centerX = static_cast<float>(w - 1) * 0.5f * hScale;
    const float centerZ = static_cast<float>(d - 1) * 0.5f * hScale;
    const float maxRadius =
        std::min(static_cast<float>(w - 1), static_cast<float>(d - 1)) * 0.5f * hScale;
    const float baseFreq = std::max(0.00001f, settings.noise.frequency);
    const float warpFreqScale = settings.noise.warpFrequency / baseFreq;

    std::function<void(size_t)> eval = [&](size_t ni) {
        if (visited[ni])
            return;

        const CompiledNode& cn = compiled.nodes[ni];

        // Evaluate inputs first
        for (uint16_t src : cn.inputs) {
            eval(src);
        }

        if (cn.kind == NodeKind::Fbm || cn.kind == NodeKind::RidgedFbm) {
            const auto& np = std::get<NoiseParams>(cn.params);
            const float freq = np.frequency;
            const int oct = np.octaves;
            const float lac = np.lacunarity;
            const float gn = np.gain;
            const float sharp = np.sharpness;
            const float xOff = np.xOffset;
            const float zOff = np.zOffset;
            const bool remap = np.remapToUnit;

            auto& out = nodeOutputs[ni];
            out.assign(cellCount, 0.0f);

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float wx = static_cast<float>(x) * hScale + xOff;
                    const float wz = static_cast<float>(z) * hScale + zOff;

                    if (cn.kind == NodeKind::Fbm) {
                        out[idx] = noiseContext.fbm(wx, wz, oct, lac, gn, freq);
                    } else {
                        out[idx] = noiseContext.ridgedFbm(wx, wz, oct, lac, gn, sharp, freq);
                    }

                    if (remap) {
                        out[idx] = 0.5f * (out[idx] + 1.0f);
                    }
                }
            }
        } else if (cn.kind == NodeKind::TerrainSynthesis) {
            const auto& tsp = std::get<TerrainSynthesisParams>(cn.params);
            const float vertScale = tsp.verticalScale;

            const std::vector<float>& continental = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& ridges = nodeOutputs[cn.inputs[1]];

            fields.heights.assign(cellCount, 0.0f);
            fields.mountainWeights.assign(cellCount, 0.0f);
            fields.valleyWeights.assign(cellCount, 0.0f);
            fields.plateauWeights.assign(cellCount, 0.0f);
            fields.sampleXs.assign(cellCount, 0.0f);
            fields.sampleZs.assign(cellCount, 0.0f);

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float worldX = static_cast<float>(x) * hScale;
                    const float worldZ = static_cast<float>(z) * hScale;

                    const float warpX =
                        noiseContext.fbm(worldX * warpFreqScale + 31.7f,
                                         worldZ * warpFreqScale - 18.2f, 3,
                                         settings.noise.lacunarity, 0.5f, baseFreq) *
                        settings.noise.warpAmplitude;
                    const float warpZ =
                        noiseContext.fbm(worldX * warpFreqScale - 47.1f,
                                         worldZ * warpFreqScale + 22.8f, 3,
                                         settings.noise.lacunarity, 0.5f, baseFreq) *
                        settings.noise.warpAmplitude;

                    const float sampleX = worldX + warpX;
                    const float sampleZ = worldZ + warpZ;
                    fields.sampleXs[idx] = sampleX;
                    fields.sampleZs[idx] = sampleZ;

                    const float detail = 0.5f * (noiseContext.fbm(sampleX * 2.7f, sampleZ * 2.7f, 4,
                                                                  2.0f, 0.5f, baseFreq) +
                                                 1.0f);

                    const float rangeMask = smoothstep(
                        0.42f, 0.72f,
                        0.5f * (noiseContext.fbm(sampleX * 0.3f + 400.0f, sampleZ * 0.3f - 250.0f,
                                                 3, settings.noise.lacunarity, 0.45f, baseFreq) +
                                1.0f));

                    const float slopeHint =
                        std::clamp((ridges[idx] - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);

                    const MountainResult mountain = computeMountain(
                        {continental[idx], ridges[idx], detail, slopeHint, rangeMask, vertScale});

                    const float basin =
                        0.5f * (noiseContext.fbm(sampleX * 0.28f - 191.7f, sampleZ * 0.28f + 83.4f,
                                                 3, settings.noise.lacunarity, 0.52f, baseFreq) +
                                1.0f);
                    const float detailBand =
                        0.5f * (noiseContext.fbm(sampleX * 1.9f + 52.3f, sampleZ * 1.9f - 61.8f,
                                                 std::max(3, settings.noise.octaves - 2),
                                                 settings.noise.lacunarity, settings.noise.gain,
                                                 baseFreq) +
                                1.0f);
                    const float rimMask = smoothstep(
                        0.38f, 0.74f,
                        0.5f * (noiseContext.fbm(sampleX * 0.17f + 420.0f, sampleZ * 0.17f - 301.0f,
                                                 3, settings.noise.lacunarity, 0.45f, baseFreq) +
                                1.0f));
                    const float valleySlopeHint =
                        std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);

                    const ValleyResult valley = computeValley(
                        {continental[idx], basin, detailBand, valleySlopeHint, rimMask, vertScale});

                    const int plainsOctaves = std::max(3, settings.noise.octaves - 2);
                    const float plainsBase =
                        0.5f * (noiseContext.fbm(sampleX - 63.2f, sampleZ + 41.8f, plainsOctaves,
                                                 settings.noise.lacunarity, settings.noise.gain,
                                                 baseFreq) +
                                1.0f);
                    const float macroRelief =
                        0.5f * (noiseContext.fbm(sampleX * 0.30f + 219.4f, sampleZ * 0.30f - 174.6f,
                                                 3, settings.noise.lacunarity, 0.48f, baseFreq) +
                                1.0f);
                    const float hilliness =
                        0.5f * (noiseContext.fbm(sampleX * 0.82f - 141.5f, sampleZ * 0.82f + 96.8f,
                                                 std::max(3, settings.noise.octaves - 1),
                                                 settings.noise.lacunarity, settings.noise.gain,
                                                 baseFreq) +
                                1.0f);
                    const float basinNoise =
                        0.5f * (noiseContext.fbm(sampleX * 0.18f - 331.7f, sampleZ * 0.18f + 271.4f,
                                                 2, settings.noise.lacunarity, 0.55f, baseFreq) +
                                1.0f);
                    const float plainsHeight =
                        computePlainsHeight({continental[idx], plainsBase, macroRelief, hilliness,
                                             basinNoise, detail, vertScale});

                    const float plateauMask =
                        0.5f * (noiseContext.fbm(sampleX * settings.plateaus.frequency,
                                                 sampleZ * settings.plateaus.frequency, 3,
                                                 settings.noise.lacunarity, 0.52f, baseFreq) +
                                1.0f);
                    const PlateauResult plateau =
                        computePlateau({continental[idx], plateauMask, detail, vertScale});

                    const float falloff =
                        islandFalloff(settings, worldX, worldZ, centerX, centerZ, maxRadius);
                    const BlendResult blend = blendTerrain(
                        {mountain.height, mountain.weight, plainsHeight, plateau.height,
                         plateau.weight, valley.depth, detail, falloff, vertScale});

                    fields.heights[idx] = blend.height;
                    fields.mountainWeights[idx] = blend.mountainWeight;
                    fields.valleyWeights[idx] = valley.weight;
                    fields.plateauWeights[idx] = plateau.weight;
                }
            }
        }
        visited[ni] = true;
    };

    eval(rootIdx);
    return fields;
}

} // namespace graph
