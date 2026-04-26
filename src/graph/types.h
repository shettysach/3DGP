#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace graph {

// === Ids ===

using NodeId = int32_t;
using LinkId = int32_t;

// === Node Kinds ===

enum class NodeKind : uint8_t {
    Fbm,
    RidgedFbm,
    TerrainSynthesis,
};

// === Pin References ===

struct PinRef {
    NodeId nodeId = 0;
    uint8_t slot  = 0;
};

// === Params ===

struct NoiseParams {
    float frequency   = 0.007f;
    int   octaves     = 6;
    float lacunarity  = 2.0f;
    float gain        = 0.5f;
    float sharpness   = 2.0f;
    float xOffset     = 0.0f;
    float zOffset     = 0.0f;
    bool  remapToUnit = true;
};

struct TerrainSynthesisParams {
    float verticalScale  = 80.0f;
    bool  islandFalloff  = true;
    float falloffRadius  = 0.9f;
    float falloffPower   = 2.2f;
};

using NodeParams = std::variant<NoiseParams, TerrainSynthesisParams>;

// === Editor Graph Model ===

struct EditorNode {
    NodeId   id    = 0;
    NodeKind kind  = NodeKind::Fbm;
    float    posX  = 0.0f;
    float    posY  = 0.0f;
    NodeParams params;
};

struct EditorLink {
    LinkId id = 0;
    PinRef from;
    PinRef to;
};

struct EditorGraph {
    std::vector<EditorNode> nodes;
    std::vector<EditorLink> links;
};

// === Compiled Graph ===

struct InputBinding {
    uint16_t sourceNodeIndex   = 0;
    uint8_t  sourceOutputSlot  = 0;
};

struct CompiledNode {
    NodeKind kind;
    NodeParams params;
    std::vector<InputBinding> inputs;
};

enum class FieldSlot : uint8_t {
    Height,
    MountainWeight,
    ValleyWeight,
    PlateauWeight,
    SampleX,
    SampleZ,
};

struct OutputBinding {
    FieldSlot slot;
    uint16_t  sourceNodeIndex  = 0;
    uint8_t   sourceOutputSlot = 0;
};

struct CompiledGraph {
    std::vector<CompiledNode>  nodes;
    std::vector<OutputBinding> outputs;
};

// === Node Definition Table ===

struct PinDef {
    const char* label;
    bool        isInput = false;
};

struct NodeDef {
    NodeKind kind;
    const char* name;
    std::vector<PinDef> inputs;
    std::vector<PinDef> outputs;
};

// Returns static pin layout for a given node kind.
const NodeDef& nodeDefinition(NodeKind kind);

// Returns default params for a given node kind.
NodeParams defaultParams(NodeKind kind);

// Builds the default graph (Fbm + RidgedFbm → TerrainSynthesis).
EditorGraph defaultGraph();

} // namespace graph

#endif // GRAPH_TYPES_H
