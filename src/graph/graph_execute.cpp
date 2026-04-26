#include "graph/graph_execute.h"
#include "graph/types.h"
#include "../terrain/blending.h"
#include "../terrain/fields.h"
#include "../terrain/mountains.h"
#include "../terrain/plains.h"
#include "../terrain/plateaus.h"
#include "../terrain/util.h"
#include "../terrain/valleys.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace graph {

using terrain::blendTerrain;
using terrain::BlendResult;
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

float islandFalloff(
    const terrain::TerrainSettings& settings,
    float worldX, float worldZ,
    float centerX, float centerZ, float maxRadius) {
    if (!settings.islandFalloff) return 1.0f;
    const float dx = worldX - centerX;
    const float dz = worldZ - centerZ;
    const float radius = std::sqrt(dx * dx + dz * dz);
    const float scaled = std::max(0.0001f, maxRadius * settings.falloffRadius);
    float t = 1.0f - radius / scaled;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::pow(t, settings.falloffPower);
}

} // namespace

terrain::TerrainFields execute(
    const CompiledGraph& compiled,
    const terrain::TerrainSettings& settings,
    const terrain::NoiseContext& noiseContext) {

    const int w = settings.width;
    const int d = settings.depth;
    const float hScale = settings.horizontalScale;
    const size_t cellCount = static_cast<size_t>(w) * static_cast<size_t>(d);

    // Per-node output buffers: [nodeIndex][outputSlot] = field buffer
    std::vector<std::vector<std::vector<float>>> nodeOutputs(compiled.nodes.size());

    // Eval each node in compiled order
    for (size_t ni = 0; ni < compiled.nodes.size(); ++ni) {
        const CompiledNode& cn = compiled.nodes[ni];
        const NodeDef& nd = nodeDefinition(cn.kind);
        const size_t numOuts = nd.outputs.size();

        // Allocate output buffers
        nodeOutputs[ni].resize(numOuts);
        for (size_t s = 0; s < numOuts; ++s) {
            nodeOutputs[ni][s].assign(cellCount, 0.0f);
        }

        if (cn.kind == NodeKind::Fbm || cn.kind == NodeKind::RidgedFbm) {
            // --- Noise node ---
            const auto& np = std::get<NoiseParams>(cn.params);
            const float freq = np.frequency;
            const int oct = np.octaves;
            const float lac = np.lacunarity;
            const float gn = np.gain;
            const float sharp = np.sharpness;
            const float xOff = np.xOffset;
            const float zOff = np.zOffset;
            const bool remap = np.remapToUnit;

            auto& out = nodeOutputs[ni][0];

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
            // --- TerrainSynthesis node ---
            const auto& tsp = std::get<TerrainSynthesisParams>(cn.params);
            const float vertScale = tsp.verticalScale;

            // Resolve input field buffers
            const std::vector<float>* continentalPtr = nullptr;
            const std::vector<float>* ridgesPtr = nullptr;

            for (size_t is = 0; is < cn.inputs.size(); ++is) {
                const InputBinding& ib = cn.inputs[is];
                const std::vector<float>& src = nodeOutputs[ib.sourceNodeIndex][ib.sourceOutputSlot];
                // Pin slot 0 → continental, slot 1 → ridges
                if (is == 0) continentalPtr = &src;
                if (is == 1) ridgesPtr = &src;
            }

            // If no continental input linked, use a default constant field of 0.5
            std::vector<float> defaultConstant;
            if (!continentalPtr) {
                defaultConstant.assign(cellCount, 0.5f);
                continentalPtr = &defaultConstant;
            }
            if (!ridgesPtr) {
                // Use a different default for ridges
                static std::vector<float> ridgedDefault;
                if (ridgedDefault.empty()) {
                    ridgedDefault.assign(cellCount, 0.5f);
                }
                ridgesPtr = &ridgedDefault;
            }

            const std::vector<float>& continental = *continentalPtr;
            const std::vector<float>& ridges = *ridgesPtr;

            // Access output buffers by name convention
            auto& outHeight        = nodeOutputs[ni][0];
            auto& outMountainWt    = nodeOutputs[ni][1];
            auto& outValleyWt      = nodeOutputs[ni][2];
            auto& outPlateauWt     = nodeOutputs[ni][3];
            auto& outSampleX       = nodeOutputs[ni][4];
            auto& outSampleZ       = nodeOutputs[ni][5];

            const float centerX = static_cast<float>(w - 1) * 0.5f * hScale;
            const float centerZ = static_cast<float>(d - 1) * 0.5f * hScale;
            const float maxRadius = std::min(static_cast<float>(w - 1), static_cast<float>(d - 1)) * 0.5f * hScale;

            const float baseFreq = std::max(0.00001f, settings.noise.frequency);
            const float warpFreqScale = settings.noise.warpFrequency / baseFreq;

            // Hardcoded constants matching buildBaseTerrainFields()
            for (int z = 0; z < d; ++z) {
                for (int x = 0; x < w; ++x) {
                    const size_t idx = fieldIndex(x, z, w);
                    const float worldX = static_cast<float>(x) * hScale;
                    const float worldZ = static_cast<float>(z) * hScale;

                    // Warp (hardcoded)
                    const float warpX = noiseContext.fbm(worldX * warpFreqScale + 31.7f,
                                                          worldZ * warpFreqScale - 18.2f,
                                                          3, settings.noise.lacunarity, 0.5f, baseFreq) *
                                        settings.noise.warpAmplitude;
                    const float warpZ = noiseContext.fbm(worldX * warpFreqScale - 47.1f,
                                                          worldZ * warpFreqScale + 22.8f,
                                                          3, settings.noise.lacunarity, 0.5f, baseFreq) *
                                        settings.noise.warpAmplitude;

                    const float sampleX = worldX + warpX;
                    const float sampleZ = worldZ + warpZ;
                    outSampleX[idx] = sampleX;
                    outSampleZ[idx] = sampleZ;

                    // Detail (hardcoded FBM)
                    const float detail = 0.5f * (noiseContext.fbm(sampleX * 2.7f, sampleZ * 2.7f,
                                                                    4, 2.0f, 0.5f, baseFreq) + 1.0f);

                    // rangeMask (hardcoded)
                    const float rangeMask = smoothstep(
                        0.42f, 0.72f,
                        0.5f * (noiseContext.fbm(sampleX * 0.3f + 400.0f, sampleZ * 0.3f - 250.0f,
                                                   3, settings.noise.lacunarity, 0.45f, baseFreq) + 1.0f));

                    const float slopeHint = std::clamp((ridges[idx] - 0.35f) * 1.55f + detail * 0.2f, 0.0f, 1.0f);

                    const MountainResult mountain = computeMountain(
                        {continental[idx], ridges[idx], detail, slopeHint, rangeMask, vertScale});

                    // basin (hardcoded)
                    const float basin = 0.5f * (noiseContext.fbm(sampleX * 0.28f - 191.7f, sampleZ * 0.28f + 83.4f,
                                                                   3, settings.noise.lacunarity, 0.52f, baseFreq) + 1.0f);
                    const float detailBand = 0.5f * (noiseContext.fbm(
                                                         sampleX * 1.9f + 52.3f, sampleZ * 1.9f - 61.8f,
                                                         std::max(3, settings.noise.octaves - 2),
                                                         settings.noise.lacunarity, settings.noise.gain, baseFreq) + 1.0f);
                    const float rimMask = smoothstep(
                        0.38f, 0.74f,
                        0.5f * (noiseContext.fbm(sampleX * 0.17f + 420.0f, sampleZ * 0.17f - 301.0f,
                                                   3, settings.noise.lacunarity, 0.45f, baseFreq) + 1.0f));
                    const float valleySlopeHint = std::clamp((0.62f - basin) * 1.35f + detail * 0.22f, 0.0f, 1.0f);

                    const ValleyResult valley = computeValley(
                        {continental[idx], basin, detailBand, valleySlopeHint, rimMask, vertScale});

                    // Plains (hardcoded)
                    const int plainsOctaves = std::max(3, settings.noise.octaves - 2);
                    const float plainsBase = 0.5f * (noiseContext.fbm(
                                                         sampleX - 63.2f, sampleZ + 41.8f,
                                                         plainsOctaves, settings.noise.lacunarity,
                                                         settings.noise.gain, baseFreq) + 1.0f);
                    const float macroRelief = 0.5f * (noiseContext.fbm(
                                                          sampleX * 0.30f + 219.4f, sampleZ * 0.30f - 174.6f,
                                                          3, settings.noise.lacunarity, 0.48f, baseFreq) + 1.0f);
                    const float hilliness = 0.5f * (noiseContext.fbm(
                                                        sampleX * 0.82f - 141.5f, sampleZ * 0.82f + 96.8f,
                                                        std::max(3, settings.noise.octaves - 1),
                                                        settings.noise.lacunarity, settings.noise.gain, baseFreq) + 1.0f);
                    const float basinNoise = 0.5f * (noiseContext.fbm(
                                                         sampleX * 0.18f - 331.7f, sampleZ * 0.18f + 271.4f,
                                                         2, settings.noise.lacunarity, 0.55f, baseFreq) + 1.0f);
                    const float plainsHeight = computePlainsHeight(
                        {continental[idx], plainsBase, macroRelief, hilliness, basinNoise, detail, vertScale});

                    // Plateau (hardcoded)
                    const float plateauMask = 0.5f * (noiseContext.fbm(
                                                          sampleX * settings.plateaus.frequency,
                                                          sampleZ * settings.plateaus.frequency,
                                                          3, settings.noise.lacunarity, 0.52f, baseFreq) + 1.0f);
                    const PlateauResult plateau = computePlateau(
                        {continental[idx], plateauMask, detail, vertScale});

                    // Blend
                    const float falloff = islandFalloff(settings, worldX, worldZ, centerX, centerZ, maxRadius);
                    const BlendResult blend = blendTerrain(
                        {mountain.height, mountain.weight, plainsHeight, plateau.height,
                         plateau.weight, valley.depth, detail, falloff, vertScale});

                    outHeight[idx]     = blend.height;
                    outMountainWt[idx] = blend.mountainWeight;
                    outValleyWt[idx]   = valley.weight;
                    outPlateauWt[idx]  = plateau.weight;
                }
            }
        } else {
            throw std::runtime_error("Unknown node kind in compiled graph");
        }
    }

    // ----- Wire outputs into TerrainFields -----

    terrain::TerrainFields fields(w, d);

    for (const auto& ob : compiled.outputs) {
        const std::vector<float>& src = nodeOutputs[ob.sourceNodeIndex][ob.sourceOutputSlot];

        switch (ob.slot) {
        case FieldSlot::Height:         fields.heights          = src; break;
        case FieldSlot::MountainWeight: fields.mountainWeights  = src; break;
        case FieldSlot::ValleyWeight:   fields.valleyWeights    = src; break;
        case FieldSlot::PlateauWeight:  fields.plateauWeights   = src; break;
        case FieldSlot::SampleX:        fields.sampleXs         = src; break;
        case FieldSlot::SampleZ:        fields.sampleZs         = src; break;
        }
    }

    return fields;
}

} // namespace graph
