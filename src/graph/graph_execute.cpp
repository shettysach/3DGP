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

// Hard-coded warp constants (replaces settings.noise.warpFrequency / warpAmplitude)
constexpr float kWarpFreqScale = 0.003f / 0.007f; // ≈ 0.4286
constexpr float kWarpAmplitude = 45.0f;
constexpr float kBaseFreq = 0.007f;
constexpr float kWarpLacunarity = 2.0f;
constexpr float kWarpGain = 0.5f;

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

    std::function<void(size_t)> eval = [&](size_t ni) {
        if (visited[ni])
            return;

        const CompiledNode& cn = compiled.nodes[ni];

        // Evaluate inputs first
        for (uint16_t src : cn.inputs) {
            eval(src);
        }

        const uint8_t ch = cn.channelCount;
        auto& out = nodeOutputs[ni];
        out.assign(cellCount * ch, 0.0f);

        if (cn.kind == NodeKind::Fbm || cn.kind == NodeKind::RidgedFbm ||
            cn.kind == NodeKind::Simplex || cn.kind == NodeKind::Perlin) {
            const auto& np = std::get<NoiseParams>(cn.params);

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float wx = static_cast<float>(x) * hScale + np.xOffset;
                    const float wz = static_cast<float>(z) * hScale + np.zOffset;

                    if (cn.kind == NodeKind::Fbm) {
                        out[idx] = noiseContext.fbm(wx, wz, np.octaves, np.lacunarity, np.gain, np.frequency);
                    } else if (cn.kind == NodeKind::RidgedFbm) {
                        out[idx] = noiseContext.ridgedFbm(wx, wz, np.octaves, np.lacunarity, np.gain, np.sharpness, np.frequency);
                    } else if (cn.kind == NodeKind::Simplex) {
                        out[idx] = noiseContext.simplex2D(wx * np.frequency, wz * np.frequency);
                    } else {
                        out[idx] = noiseContext.perlin2D(wx * np.frequency, wz * np.frequency);
                    }

                    out[idx] = 0.5f * (out[idx] + 1.0f);
                }
            }
        } else if (cn.kind == NodeKind::Mountains) {
            const auto& mp = std::get<MountainParams>(cn.params);
            const std::vector<float>& continental = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& ridges       = nodeOutputs[cn.inputs[1]];
            const std::vector<float>& detailIn     = nodeOutputs[cn.inputs[2]];
            const std::vector<float>& rangeMaskIn  = nodeOutputs[cn.inputs[3]];

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float detail = detailIn[idx];
                    const float rangeMask = smoothstep(0.42f, 0.72f, rangeMaskIn[idx]);
                    const float slopeHint =
                        std::clamp((ridges[idx] - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);

                    const MountainResult mr = computeMountain(
                        {continental[idx], ridges[idx], detail, slopeHint, rangeMask, mp.verticalScale});

                    out[idx * 2 + 0] = mr.height;
                    out[idx * 2 + 1] = mr.weight;
                }
            }
        } else if (cn.kind == NodeKind::Valleys) {
            const auto& vp = std::get<ValleyParams>(cn.params);
            const std::vector<float>& continental = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& basinIn     = nodeOutputs[cn.inputs[1]];
            const std::vector<float>& detailIn    = nodeOutputs[cn.inputs[2]];
            const std::vector<float>& rimMaskIn   = nodeOutputs[cn.inputs[3]];

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float basin = basinIn[idx];
                    const float detail = detailIn[idx];
                    const float rimMask = smoothstep(0.38f, 0.74f, rimMaskIn[idx]);
                    const float valleySlopeHint =
                        std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);

                    const ValleyResult vr = computeValley(
                        {continental[idx], basin, detail, valleySlopeHint, rimMask, vp.verticalScale});

                    out[idx * 2 + 0] = vr.depth;
                    out[idx * 2 + 1] = vr.weight;
                }
            }
        } else if (cn.kind == NodeKind::Plains) {
            const auto& pp = std::get<PlainsParams>(cn.params);
            const std::vector<float>& continental = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& plainsBase  = nodeOutputs[cn.inputs[1]];
            const std::vector<float>& macroRelief = nodeOutputs[cn.inputs[2]];
            const std::vector<float>& hilliness   = nodeOutputs[cn.inputs[3]];
            const std::vector<float>& detailIn    = nodeOutputs[cn.inputs[4]];

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float height = computePlainsHeight(
                        {continental[idx], plainsBase[idx], macroRelief[idx], hilliness[idx],
                         0.5f, detailIn[idx], pp.verticalScale});
                    out[idx] = height;
                }
            }
        } else if (cn.kind == NodeKind::Plateaus) {
            const auto& pp = std::get<PlateauParams>(cn.params);
            const std::vector<float>& continental = nodeOutputs[cn.inputs[0]];
            const std::vector<float>& plateauMask = nodeOutputs[cn.inputs[1]];
            const std::vector<float>& detailIn    = nodeOutputs[cn.inputs[2]];

            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const PlateauResult pr = computePlateau(
                        {continental[idx], plateauMask[idx], detailIn[idx], pp.verticalScale});

                    out[idx * 2 + 0] = pr.height;
                    out[idx * 2 + 1] = pr.weight;
                }
            }
        } else if (cn.kind == NodeKind::TerrainSynthesis) {
            const auto& tsp = std::get<TerrainSynthesisParams>(cn.params);
            const float vertScale = tsp.verticalScale;

            const std::vector<float>& mountain = nodeOutputs[cn.inputs[0]]; // 2ch
            const std::vector<float>& valley   = nodeOutputs[cn.inputs[1]]; // 2ch
            const std::vector<float>& plains   = nodeOutputs[cn.inputs[2]]; // 1ch
            const std::vector<float>& plateau  = nodeOutputs[cn.inputs[3]]; // 2ch
            const std::vector<float>& detailIn = nodeOutputs[cn.inputs[4]]; // 1ch

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

                    // Hard-coded warp
                    const float warpX =
                        noiseContext.fbm(worldX * kWarpFreqScale + 31.7f,
                                         worldZ * kWarpFreqScale - 18.2f, 3,
                                         kWarpLacunarity, kWarpGain, kBaseFreq) *
                        kWarpAmplitude;
                    const float warpZ =
                        noiseContext.fbm(worldX * kWarpFreqScale - 47.1f,
                                         worldZ * kWarpFreqScale + 22.8f, 3,
                                         kWarpLacunarity, kWarpGain, kBaseFreq) *
                        kWarpAmplitude;

                    fields.sampleXs[idx] = worldX + warpX;
                    fields.sampleZs[idx] = worldZ + warpZ;

                    const float mountainHeight = mountain[idx * 2 + 0];
                    const float mountainWeight = mountain[idx * 2 + 1];
                    const float valleyDepth    = valley[idx * 2 + 0];
                    const float valleyWeight   = valley[idx * 2 + 1];
                    const float plainsHeight   = plains[idx];
                    const float plateauHeight  = plateau[idx * 2 + 0];
                    const float plateauWeight  = plateau[idx * 2 + 1];
                    const float detail         = detailIn[idx];

                    const float falloff =
                        islandFalloff(settings, worldX, worldZ, centerX, centerZ, maxRadius);
                    const BlendResult blend = blendTerrain(
                        {mountainHeight, mountainWeight, plainsHeight, plateauHeight,
                         plateauWeight, valleyDepth, detail, falloff, vertScale});

                    fields.heights[idx] = blend.height;
                    fields.mountainWeights[idx] = blend.mountainWeight;
                    fields.valleyWeights[idx] = valleyWeight;
                    fields.plateauWeights[idx] = plateauWeight;
                }
            }
        }
        visited[ni] = true;
    };

    eval(rootIdx);
    return fields;
}

} // namespace graph
