#include "graph/execute.h"

#include <functional>
#include <variant>
#include <vector>

#include "../terrain/blending.h"
#include "../terrain/fields.h"
#include "../terrain/mountains.h"
#include "../terrain/plains.h"
#include "../terrain/plateaus.h"
#include "../terrain/util.h"
#include "../terrain/valleys.h"
#include "graph/types.h"

namespace graph {

using terrain::BlendInput;
using terrain::BlendResult;
using terrain::blendTerrain;
using terrain::computeMountain;
using terrain::computePlainsHeight;
using terrain::computePlateau;
using terrain::computeValley;
using terrain::fieldIndex;
using terrain::MountainInput;
using terrain::MountainResult;
using terrain::PlainsInput;
using terrain::PlateauInput;
using terrain::PlateauResult;
using terrain::smoothstep;
using terrain::lerp;
using terrain::computeTerrace;
using terrain::ValleyInput;
using terrain::ValleyResult;

using NodeOutput =
    std::variant<std::monostate, std::vector<float>, std::vector<Vec2>>;

namespace {
    constexpr float kDetailFreq = 0.0189f;
    constexpr float kMacroReliefFreq = 0.0021f;
    constexpr float kHillinessFreq = 0.00574f;
    constexpr float kBasinNoiseFreq = 0.00126f;
} // namespace

terrain::TerrainFields execute(
    const CompiledGraph& compiled,
    const terrain::TerrainSettings& settings,
    const terrain::NoiseContext& noiseContext
) {
    const size_t N = compiled.nodes.size();
    const int w = settings.width;
    const int d = settings.depth;
    const float hScale = settings.horizontalScale;
    const size_t cellCount = static_cast<size_t>(w) * static_cast<size_t>(d);

    size_t rootIdx = 0;
    for (size_t i = 0; i < N; ++i) {
        if (compiled.nodes[i].kind == NodeKind::Blend) {
            rootIdx = i;
            break;
        }
    }

    std::vector<NodeOutput> nodeOutputs(N);
    std::vector<bool> visited(N, false);
    terrain::TerrainFields fields(w, d);

    const std::vector<float> zeroFloat(cellCount, 0.0f);
    const std::vector<Vec2> zeroVec2(cellCount, Vec2 {0.0f, 0.0f});

    auto getFloat = [&](const CompiledNode& cn,
                        size_t slot) -> const std::vector<float>& {
        if (slot < cn.inputs.size() && cn.inputs[slot].has_value())
            return std::get<std::vector<float>>(nodeOutputs[*cn.inputs[slot]]);
        return zeroFloat;
    };

    auto getVec2 = [&](const CompiledNode& cn,
                       size_t slot) -> const std::vector<Vec2>& {
        if (slot < cn.inputs.size() && cn.inputs[slot].has_value())
            return std::get<std::vector<Vec2>>(nodeOutputs[*cn.inputs[slot]]);
        return zeroVec2;
    };

    std::function<void(size_t)> eval = [&](size_t ni) {
        if (visited[ni])
            return;

        const CompiledNode& cn = compiled.nodes[ni];

        for (const auto& src : cn.inputs) {
            if (src.has_value())
                eval(*src);
        }

        switch (cn.kind) {
            case NodeKind::Fbm:
            case NodeKind::RidgedFbm:
            case NodeKind::FractalPerlin:
            case NodeKind::Perlin:
            case NodeKind::Simplex: {
                const auto& np = std::get<NoiseParams>(cn.params);

                auto noiseVal = [&](float wx, float wz) -> float {
                    switch (cn.kind) {
                        case NodeKind::Fbm:
                            return noiseContext.fbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.frequency
                            );
                        case NodeKind::RidgedFbm:
                            return noiseContext.ridgedFbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.sharpness,
                                np.frequency
                            );
                        case NodeKind::FractalPerlin:
                            return noiseContext.perlinFbm(
                                wx,
                                wz,
                                np.octaves,
                                np.lacunarity,
                                np.gain,
                                np.frequency
                            );
                        case NodeKind::Perlin:
                            return noiseContext.perlin2D(
                                wx * np.frequency,
                                wz * np.frequency
                            );
                        default: // Simplex
                            return noiseContext.simplex2D(
                                wx * np.frequency,
                                wz * np.frequency
                            );
                    }
                };

                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);

                if (!cn.inputs.empty() && cn.inputs[0].has_value()) {
                    const auto& coordBuf =
                        std::get<std::vector<Vec2>>(nodeOutputs[*cn.inputs[0]]);
                    for (int z = 0; z < d; ++z) {
                        for (int x = 0; x < w; ++x) {
                            const size_t idx = fieldIndex(x, z, w);
                            const float wx = coordBuf[idx].x + np.xOffset;
                            const float wz = coordBuf[idx].y + np.zOffset;
                            out[idx] = 0.5f * (noiseVal(wx, wz) + 1.0f);
                        }
                    }
                } else {
                    for (int z = 0; z < d; ++z) {
                        for (int x = 0; x < w; ++x) {
                            const size_t idx = fieldIndex(x, z, w);
                            const float wx =
                                static_cast<float>(x) * hScale + np.xOffset;
                            const float wz =
                                static_cast<float>(z) * hScale + np.zOffset;
                            out[idx] = 0.5f * (noiseVal(wx, wz) + 1.0f);
                        }
                    }
                }
                break;
            }

            case NodeKind::Position: {
                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        out[idx] = Vec2 {
                            static_cast<float>(x) * hScale,
                            static_cast<float>(z) * hScale
                        };
                    }
                }
                break;
            }

            case NodeKind::CreateVec2: {
                const auto& cp = std::get<CreateVec2Params>(cn.params);
                const bool hasX = cn.inputs[0].has_value();
                const bool hasY = cn.inputs[1].has_value();
                const float* px = nullptr;
                const float* py = nullptr;
                if (hasX)
                    px =
                        std::get<std::vector<float>>(nodeOutputs[*cn.inputs[0]])
                            .data();
                if (hasY)
                    py =
                        std::get<std::vector<float>>(nodeOutputs[*cn.inputs[1]])
                            .data();

                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = Vec2 {
                        px ? px[i] + cp.x : cp.x,
                        py ? py[i] + cp.y : cp.y
                    };
                }
                break;
            }

            case NodeKind::Add2: {
                const auto& a = getVec2(cn, 0);
                const auto& b = getVec2(cn, 1);
                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = Vec2 {a[i].x + b[i].x, a[i].y + b[i].y};
                }
                break;
            }

            case NodeKind::Scale2: {
                const auto& sp = std::get<Scale2Params>(cn.params);
                const auto& in = getVec2(cn, 0);
                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = Vec2 {in[i].x * sp.scale, in[i].y * sp.scale};
                }
                break;
            }

            case NodeKind::Mountain: {
                const auto& mp = std::get<MountainParams>(cn.params);
                const float vertScale = settings.verticalScale;

                const auto& continental = getFloat(cn, 0);
                const auto& ridges = getFloat(cn, 1);
                const auto& rangeMaskIn = getFloat(cn, 2);

                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float wx = static_cast<float>(x) * hScale;
                        const float wz = static_cast<float>(z) * hScale;

                        const float detail = 0.5f
                            * (noiseContext.simplex2D(
                                   wx * kDetailFreq,
                                   wz * kDetailFreq
                               )
                               + 1.0f);

                        MountainInput mtnIn {
                            continental[idx],
                            ridges[idx],
                            detail,
                            rangeMaskIn[idx],
                            vertScale,
                            mp
                        };
                        const MountainResult r = computeMountain(mtnIn);

                        out[idx] = Vec2 {r.height, r.weight};
                    }
                }
                break;
            }

            case NodeKind::Valley: {
                const auto& vp = std::get<ValleyParams>(cn.params);
                const float vertScale = settings.verticalScale;

                const auto& continental = getFloat(cn, 0);
                const auto& basin = getFloat(cn, 1);
                const auto& rimMaskIn = getFloat(cn, 2);

                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float wx = static_cast<float>(x) * hScale;
                        const float wz = static_cast<float>(z) * hScale;

                        const float detail = 0.5f
                            * (noiseContext.simplex2D(
                                   wx * kDetailFreq,
                                   wz * kDetailFreq
                               )
                               + 1.0f);

                        ValleyInput vIn {
                            continental[idx],
                            basin[idx],
                            detail,
                            rimMaskIn[idx],
                            vertScale,
                            vp
                        };
                        const ValleyResult r = computeValley(vIn);

                        out[idx] = Vec2 {r.depth, r.weight};
                    }
                }
                break;
            }

            case NodeKind::Plains: {
                const auto& pp = std::get<PlainsParams>(cn.params);
                const float vertScale = settings.verticalScale;

                const auto& continental = getFloat(cn, 0);
                const auto& plainsBase = getFloat(cn, 1);

                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float wx = static_cast<float>(x) * hScale;
                        const float wz = static_cast<float>(z) * hScale;

                        const float detail = 0.5f
                            * (noiseContext.simplex2D(
                                   wx * kDetailFreq,
                                   wz * kDetailFreq
                               )
                               + 1.0f);

                        const float rawMacro = noiseContext.perlinFbm(
                            wx * kMacroReliefFreq + 219.4f * kMacroReliefFreq,
                            wz * kMacroReliefFreq - 174.6f * kMacroReliefFreq,
                            3,
                            2.0f,
                            0.48f,
                            1.0f
                        );
                        const float macroRelief = 0.5f * (rawMacro + 1.0f);

                        const float rawHill = noiseContext.fbm(
                            wx * kHillinessFreq - 141.5f * kHillinessFreq,
                            wz * kHillinessFreq + 96.8f * kHillinessFreq,
                            5,
                            2.0f,
                            0.50f,
                            1.0f
                        );
                        const float hilliness = 0.5f * (rawHill + 1.0f);

                        const float rawBasinN = noiseContext.simplex2D(
                            wx * kBasinNoiseFreq - 331.7f * kBasinNoiseFreq,
                            wz * kBasinNoiseFreq + 271.4f * kBasinNoiseFreq
                        );
                        const float basinNoise = 0.5f * (rawBasinN + 1.0f);

                        PlainsInput pIn {
                            continental[idx],
                            plainsBase[idx],
                            macroRelief,
                            hilliness,
                            basinNoise,
                            detail,
                            vertScale,
                            pp
                        };
                        out[idx] = computePlainsHeight(pIn);
                    }
                }
                break;
            }

            case NodeKind::Plateau: {
                const auto& tp = std::get<PlateauParams>(cn.params);
                const float vertScale = settings.verticalScale;

                const auto& continental = getFloat(cn, 0);
                const auto& plateauFeature = getFloat(cn, 1);
                const auto& plateauMaskIn = getFloat(cn, 2);

                nodeOutputs[ni] = std::vector<Vec2>(cellCount);
                auto& out = std::get<std::vector<Vec2>>(nodeOutputs[ni]);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float wx = static_cast<float>(x) * hScale;
                        const float wz = static_cast<float>(z) * hScale;

                        const float detail = 0.5f
                            * (noiseContext.simplex2D(
                                   wx * kDetailFreq,
                                   wz * kDetailFreq
                               )
                               + 1.0f);

                        const float combinedMask = plateauMaskIn[idx] * 0.5f
                            + plateauFeature[idx] * 0.5f;

                        PlateauInput platIn {
                            continental[idx],
                            combinedMask,
                            detail,
                            vertScale,
                            tp
                        };
                        const PlateauResult r = computePlateau(platIn);

                        out[idx] = Vec2 {r.height, r.weight};
                    }
                }
                break;
            }

            case NodeKind::Terrace: {
                const auto& tp = std::get<TerraceParams>(cn.params);
                const auto& in = getFloat(cn, 0);
                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = computeTerrace(in[i], tp.steps);
                }
                break;
            }

            case NodeKind::Smoothstep: {
                const auto& sp = std::get<SmoothstepParams>(cn.params);
                const auto& in = getFloat(cn, 0);
                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    out[i] = smoothstep(sp.a, sp.b, in[i]);
                }
                break;
            }

            case NodeKind::Lerp: {
                const auto& lp = std::get<LerpParams>(cn.params);
                const bool hasA = cn.inputs[0].has_value();
                const bool hasB = cn.inputs[1].has_value();
                const bool hasT = cn.inputs[2].has_value();
                const float* pa = hasA ? std::get<std::vector<float>>(nodeOutputs[*cn.inputs[0]]).data() : nullptr;
                const float* pb = hasB ? std::get<std::vector<float>>(nodeOutputs[*cn.inputs[1]]).data() : nullptr;
                const float* pt = hasT ? std::get<std::vector<float>>(nodeOutputs[*cn.inputs[2]]).data() : nullptr;
                nodeOutputs[ni] = std::vector<float>(cellCount);
                auto& out = std::get<std::vector<float>>(nodeOutputs[ni]);
                for (size_t i = 0; i < cellCount; ++i) {
                    const float a = pa ? pa[i] : lp.a;
                    const float b = pb ? pb[i] : lp.b;
                    const float t = pt ? pt[i] : lp.t;
                    out[i] = lerp(a, b, t);
                }
                break;
            }

            case NodeKind::Blend: {
                const float vertScale = settings.verticalScale;

                const auto& mountain = getVec2(cn, 0);
                const auto& valley = getVec2(cn, 1);
                const auto& plains = getFloat(cn, 2);
                const auto& plateau = getVec2(cn, 3);

                fields.heights.assign(cellCount, 0.0f);
                fields.mountainWeights.assign(cellCount, 0.0f);
                fields.valleyWeights.assign(cellCount, 0.0f);
                fields.plateauWeights.assign(cellCount, 0.0f);
                fields.sampleXs.assign(cellCount, 0.0f);
                fields.sampleZs.assign(cellCount, 0.0f);

                for (int z = 0; z < d; ++z) {
                    for (int x = 0; x < w; ++x) {
                        const size_t idx = fieldIndex(x, z, w);
                        const float wx = static_cast<float>(x) * hScale;
                        const float wz = static_cast<float>(z) * hScale;

                        fields.sampleXs[idx] = wx;
                        fields.sampleZs[idx] = wz;

                        const float detail = 0.5f
                            * (noiseContext.simplex2D(
                                   wx * kDetailFreq,
                                   wz * kDetailFreq
                               )
                               + 1.0f);

                        BlendInput blIn {
                            mountain[idx].x,
                            mountain[idx].y,
                            plains[idx],
                            plateau[idx].x,
                            plateau[idx].y,
                            valley[idx].x,
                            detail,
                            vertScale
                        };
                        const BlendResult blend = blendTerrain(blIn);

                        fields.heights[idx] = blend.height;
                        fields.mountainWeights[idx] = blend.mountainWeight;
                        fields.valleyWeights[idx] = valley[idx].y;
                        fields.plateauWeights[idx] = plateau[idx].y;
                    }
                }
                break;
            }
        }
        visited[ni] = true;
    };

    eval(rootIdx);
    return fields;
}

} // namespace graph
