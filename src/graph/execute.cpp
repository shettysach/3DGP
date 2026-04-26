#include "graph/execute.h"
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

        if (cn.kind == NodeKind::Fbm || cn.kind == NodeKind::RidgedFbm ||
            cn.kind == NodeKind::FractalPerlin || cn.kind == NodeKind::Perlin ||
            cn.kind == NodeKind::Simplex) {
            const auto& np = std::get<NoiseParams>(cn.params);
            auto& out = nodeOutputs[ni];
            out.assign(cellCount, 0.0f);

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float wx = static_cast<float>(x) * hScale + np.xOffset;
                    const float wz = static_cast<float>(z) * hScale + np.zOffset;

                    if (cn.kind == NodeKind::Fbm) {
                        out[idx] = noiseContext.fbm(wx, wz, np.octaves, np.lacunarity, np.gain, np.frequency);
                    } else if (cn.kind == NodeKind::RidgedFbm) {
                        out[idx] = noiseContext.ridgedFbm(wx, wz, np.octaves, np.lacunarity, np.gain, np.sharpness, np.frequency);
                    } else if (cn.kind == NodeKind::FractalPerlin) {
                        out[idx] = noiseContext.perlinFbm(wx, wz, np.octaves, np.lacunarity, np.gain, np.frequency);
                    } else if (cn.kind == NodeKind::Perlin) {
                        out[idx] = noiseContext.perlin2D(wx * np.frequency, wz * np.frequency);
                    } else { // Simplex
                        out[idx] = noiseContext.simplex2D(wx * np.frequency, wz * np.frequency);
                    }

                    out[idx] = 0.5f * (out[idx] + 1.0f);
                }
            }
        } else if (cn.kind == NodeKind::TerrainSynthesis) {
            const auto& tsp = std::get<TerrainSynthesisParams>(cn.params);
            const float vertScale = tsp.verticalScale;

            const std::vector<float>& continental  = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& ridges       = nodeOutputs[cn.inputs[1]];
            const std::vector<float>& detailIn     = nodeOutputs[cn.inputs[2]];
            const std::vector<float>& rangeMaskIn  = nodeOutputs[cn.inputs[3]];
            const std::vector<float>& basinIn      = nodeOutputs[cn.inputs[4]];
            const std::vector<float>& detailBandIn = nodeOutputs[cn.inputs[5]];
            const std::vector<float>& rimMaskIn    = nodeOutputs[cn.inputs[6]];
            const std::vector<float>& plainsBaseIn = nodeOutputs[cn.inputs[7]];
            const std::vector<float>& macroReliefIn= nodeOutputs[cn.inputs[8]];
            const std::vector<float>& hillinessIn  = nodeOutputs[cn.inputs[9]];
            const std::vector<float>& basinNoiseIn = nodeOutputs[cn.inputs[10]];
            const std::vector<float>& plateauMaskIn= nodeOutputs[cn.inputs[11]];

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

                    // Warp is kept so that climate post-processing still
                    // sees a coherent warped coordinate space.
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

                    const float detail      = detailIn[idx];
                    const float rangeMask   = smoothstep(0.42f, 0.72f, rangeMaskIn[idx]);
                    const float basin       = basinIn[idx];
                    const float detailBand  = detailBandIn[idx];
                    const float rimMask     = smoothstep(0.38f, 0.74f, rimMaskIn[idx]);
                    const float plainsBase  = plainsBaseIn[idx];
                    const float macroRelief = macroReliefIn[idx];
                    const float hilliness   = hillinessIn[idx];
                    const float basinNoise  = basinNoiseIn[idx];
                    const float plateauMask = plateauMaskIn[idx];

                    const float slopeHint =
                        std::clamp((ridges[idx] - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);

                    const MountainResult mountain = computeMountain(
                        {continental[idx], ridges[idx], detail, slopeHint, rangeMask, vertScale});

                    const float valleySlopeHint =
                        std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);

                    const ValleyResult valley = computeValley(
                        {continental[idx], basin, detailBand, valleySlopeHint, rimMask, vertScale});

                    const float plainsHeight =
                        computePlainsHeight({continental[idx], plainsBase, macroRelief, hilliness,
                                             basinNoise, detail, vertScale});

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
