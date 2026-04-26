#include "graph/graph_compile.h"

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace graph {

namespace {

struct NodeMeta {
    NodeId id;
    const NodeDef& def;
    size_t compiledIndex = 0;
    std::vector<InputBinding> resolvedInputs;
};

} // namespace

CompiledGraph compile(const EditorGraph& editorGraph) {
    if (editorGraph.nodes.empty()) {
        throw std::runtime_error("Graph has no nodes");
    }

    // ----- Build node meta -----

    std::unordered_map<NodeId, NodeMeta> metaByNodeId;
    std::vector<NodeMeta*> orderedMeta; // index = compiled node index

    for (const auto& en : editorGraph.nodes) {
        const NodeDef& def = nodeDefinition(en.kind);
        NodeMeta meta{en.id, def, orderedMeta.size(), {}};
        meta.resolvedInputs.resize(def.inputs.size());
        metaByNodeId.emplace(en.id, meta);
        orderedMeta.push_back(&metaByNodeId.at(en.id));
    }

    // ----- Validate links -----

    // Track which input slots are already connected (nodeId -> set of input slots)
    std::unordered_map<NodeId, std::unordered_set<uint8_t>> connectedInputs;

    for (const auto& link : editorGraph.links) {
        // Resolve from/to nodes
        auto fromIt = metaByNodeId.find(link.from.nodeId);
        auto toIt   = metaByNodeId.find(link.to.nodeId);

        if (fromIt == metaByNodeId.end()) {
            throw std::runtime_error("Link " + std::to_string(link.id) +
                                     ": source node " + std::to_string(link.from.nodeId) + " not found");
        }
        if (toIt == metaByNodeId.end()) {
            throw std::runtime_error("Link " + std::to_string(link.id) +
                                     ": target node " + std::to_string(link.to.nodeId) + " not found");
        }

        const NodeDef& fromDef = fromIt->second.def;
        const NodeDef& toDef   = toIt->second.def;

        // Validate pin slots
        if (link.from.slot >= fromDef.outputs.size()) {
            throw std::runtime_error("Link " + std::to_string(link.id) +
                                     ": source output slot " + std::to_string(link.from.slot) +
                                     " out of range for node kind");
        }
        if (link.to.slot >= toDef.inputs.size()) {
            throw std::runtime_error("Link " + std::to_string(link.id) +
                                     ": target input slot " + std::to_string(link.to.slot) +
                                     " out of range for node kind");
        }

        // Check for double connection
        auto& connSet = connectedInputs[link.to.nodeId];
        if (connSet.count(link.to.slot)) {
            throw std::runtime_error("Link " + std::to_string(link.id) +
                                     ": input slot " + std::to_string(link.to.slot) +
                                     " on node " + std::to_string(link.to.nodeId) +
                                     " already connected");
        }
        connSet.insert(link.to.slot);

        // Record the binding
        InputBinding binding;
        binding.sourceNodeIndex  = static_cast<uint16_t>(fromIt->second.compiledIndex);
        binding.sourceOutputSlot = link.from.slot;

        toIt->second.resolvedInputs[link.to.slot] = binding;
    }

    // ----- Topological sort (Kahn's algorithm) -----

    // Build adjacency + indegree lists by compiled index
    const size_t N = orderedMeta.size();
    std::vector<std::vector<size_t>> successors(N);
    std::vector<int> indegree(N, 0);

    for (const auto& link : editorGraph.links) {
        size_t fromIdx = metaByNodeId.at(link.from.nodeId).compiledIndex;
        size_t toIdx   = metaByNodeId.at(link.to.nodeId).compiledIndex;
        successors[fromIdx].push_back(toIdx);
        indegree[toIdx]++;
    }

    std::deque<size_t> queue;
    for (size_t i = 0; i < N; ++i) {
        if (indegree[i] == 0) {
            queue.push_back(i);
        }
    }

    std::vector<size_t> topoOrder;
    topoOrder.reserve(N);
    while (!queue.empty()) {
        size_t u = queue.front();
        queue.pop_front();
        topoOrder.push_back(u);

        for (size_t v : successors[u]) {
            if (--indegree[v] == 0) {
                queue.push_back(v);
            }
        }
    }

    if (topoOrder.size() != N) {
        throw std::runtime_error("Graph contains a cycle");
    }

    // ----- Build compiled graph -----

    CompiledGraph compiled;

    // Build node remap: old compiled index → new topo index
    std::vector<size_t> oldToNew(N, 0);
    for (size_t newIdx = 0; newIdx < N; ++newIdx) {
        oldToNew[topoOrder[newIdx]] = newIdx;
    }

    for (size_t newIdx : topoOrder) {
        const EditorNode& en = editorGraph.nodes[newIdx];
        const NodeMeta& meta = metaByNodeId.at(en.id);

        CompiledNode cn;
        cn.kind   = en.kind;
        cn.params = en.params;
        cn.inputs = meta.resolvedInputs;

        // Remap input bindings to use new topological indices
        for (auto& ib : cn.inputs) {
            ib.sourceNodeIndex = static_cast<uint16_t>(oldToNew.at(ib.sourceNodeIndex));
        }

        compiled.nodes.push_back(std::move(cn));
    }

    // ----- Build output bindings -----

    // Find the TerrainSynthesis node in the compiled order
    // Its outputs map to FieldSlots by position
    for (size_t i = 0; i < compiled.nodes.size(); ++i) {
        if (compiled.nodes[i].kind == NodeKind::TerrainSynthesis) {
            const NodeDef& def = nodeDefinition(NodeKind::TerrainSynthesis);
            for (uint8_t slot = 0; slot < static_cast<uint8_t>(def.outputs.size()); ++slot) {
                OutputBinding ob;
                ob.slot             = static_cast<FieldSlot>(slot);
                ob.sourceNodeIndex  = static_cast<uint16_t>(i);
                ob.sourceOutputSlot = slot;
                compiled.outputs.push_back(ob);
            }
            break;
        }
    }

    return compiled;
}

} // namespace graph
