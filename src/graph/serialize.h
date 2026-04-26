#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "graph/types.h"

#include <string>

namespace graph {

// Serialize EditorGraph to JSON string.
std::string toJson(const EditorGraph& g);

// Parse JSON string into EditorGraph. Throws std::runtime_error on failure.
EditorGraph fromJson(const std::string& json);

// Convert NodeKind ↔ string for JSON keys.
const char* kindToString(NodeKind kind);
NodeKind kindFromString(const std::string& s);

} // namespace graph

#endif // SERIALIZE_H
