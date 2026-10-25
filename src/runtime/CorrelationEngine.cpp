/**
 * CorrelationEngine.cpp
 *
 * Phase 14: Concrete Runtime ↔ Static Analysis Correlation Engine
 *
 * DOMAIN SEPARATION: This file reads two distinct event streams and
 * correlates them by source line number. Concrete addresses and abstract
 * NodeIDs are NEVER mixed. The alloc_site_map is strictly a visualization
 * aid and is explicitly labeled as such in the output JSON.
 *
 * JSON parsing: minimal hand-written parser tuned for the exact formats
 * produced by runtime_events.c and trace_driver.cpp. nlohmann/json is
 * fetched via CMake FetchContent if available; otherwise falls back to
 * simple string-based parsing.
 */

#include "mdehra/runtime/CorrelationEngine.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <cassert>
#include <set>

namespace phase14 {

// ============================================================
// Minimal JSON helpers (no external dependency required)
// ============================================================

// Read entire file into string
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Extract the value of a simple "key": "value" or "key": number field
static std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t kp = json.find(pat);
    if (kp == std::string::npos) return "";
    size_t colon = json.find(':', kp + pat.size());
    if (colon == std::string::npos) return "";
    size_t vstart = json.find_first_not_of(" \t\n\r", colon + 1);
    if (vstart == std::string::npos) return "";
    if (json[vstart] == '"') {
        size_t end = json.find('"', vstart + 1);
        while (end != std::string::npos && json[end-1] == '\\')
            end = json.find('"', end + 1);
        if (end == std::string::npos) return "";
        return json.substr(vstart + 1, end - vstart - 1);
    }
    // numeric
    size_t end = json.find_first_of(",}\n", vstart);
    return json.substr(vstart, end - vstart);
}

static int jsonGetInt(const std::string& json, const std::string& key, int def = 0) {
    std::string s = jsonGetString(json, key);
    if (s.empty()) return def;
    try { return std::stoi(s); } catch(...) { return def; }
}

static bool jsonGetBool(const std::string& json, const std::string& key, bool def = false) {
    std::string pat = "\"" + key + "\"";
    size_t kp = json.find(pat);
    if (kp == std::string::npos) return def;
    size_t colon = json.find(':', kp + pat.size());
    if (colon == std::string::npos) return def;
    size_t vstart = json.find_first_not_of(" \t\n\r", colon + 1);
    if (vstart == std::string::npos) return def;
    if (json.substr(vstart, 4) == "true") return true;
    if (json.substr(vstart, 5) == "false") return false;
    return def;
}

// Split a JSON array of objects into individual object strings
static std::vector<std::string> splitJsonArray(const std::string& json,
                                                const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t kp = json.find(pat);
    if (kp == std::string::npos) return {};
    size_t arrStart = json.find('[', kp);
    if (arrStart == std::string::npos) return {};

    std::vector<std::string> result;
    int depth = 0;
    size_t objStart = std::string::npos;
    for (size_t i = arrStart; i < json.size(); i++) {
        char c = json[i];
        if (c == '{') {
            if (depth == 0) objStart = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && objStart != std::string::npos) {
                result.push_back(json.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return result;
}

// Extract integer array like "nodes": [1, 2, 3]
static std::vector<int> jsonGetIntArray(const std::string& json,
                                         const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t kp = json.find(pat);
    if (kp == std::string::npos) return {};
    size_t arrStart = json.find('[', kp);
    size_t arrEnd   = json.find(']', arrStart);
    if (arrStart == std::string::npos || arrEnd == std::string::npos) return {};
    std::string content = json.substr(arrStart + 1, arrEnd - arrStart - 1);
    std::vector<int> result;
    std::istringstream ss(content);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // trim
        size_t s = token.find_first_not_of(" \t\n\r");
        if (s == std::string::npos) continue;
        token = token.substr(s);
        try { result.push_back(std::stoi(token)); } catch(...) {}
    }
    return result;
}

// Extract string array like "before": ["x", "y"]
static std::vector<std::string> jsonGetStringArray(const std::string& json,
                                                    const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t kp = json.find(pat);
    if (kp == std::string::npos) return {};
    size_t arrStart = json.find('[', kp);
    size_t arrEnd   = json.find(']', arrStart);
    if (arrStart == std::string::npos || arrEnd == std::string::npos) return {};
    std::string content = json.substr(arrStart + 1, arrEnd - arrStart - 1);
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t q1 = content.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(content.substr(q1 + 1, q2 - q1 - 1));
        pos = q2 + 1;
    }
    return result;
}

// ============================================================
// Parse HRA state from a JSON object substring
// ============================================================
static StaticHRAState parseHRAState(const std::string& stateJson) {
    StaticHRAState state;
    if (stateJson.empty() || stateJson == "null") return state;

    // Parse nodes
    state.nodes = jsonGetIntArray(stateJson, "nodes");

    // Parse edges: [{"source":1,"field":"next","target":2}, ...]
    auto edgeObjs = splitJsonArray(stateJson, "edges");
    for (const auto& eo : edgeObjs) {
        StaticStateEdge e;
        e.source = jsonGetInt(eo, "source");
        e.field  = jsonGetString(eo, "field");
        e.target = jsonGetInt(eo, "target");
        state.edges.push_back(e);
    }

    // Parse bindings: {"var": [node_ids...], ...}
    // Find the bindings object
    size_t bkey = stateJson.find("\"bindings\"");
    if (bkey != std::string::npos) {
        size_t bstart = stateJson.find('{', bkey + 10);
        if (bstart != std::string::npos) {
            // Find matching close brace
            int depth = 0;
            size_t bend = bstart;
            for (size_t i = bstart; i < stateJson.size(); i++) {
                if (stateJson[i] == '{') depth++;
                else if (stateJson[i] == '}') { depth--; if (depth == 0) { bend = i; break; } }
            }
            std::string bindingsStr = stateJson.substr(bstart + 1, bend - bstart - 1);
            // Each key-value pair: "varname": [n1, n2, ...]
            size_t pos = 0;
            while (pos < bindingsStr.size()) {
                size_t q1 = bindingsStr.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = bindingsStr.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string varName = bindingsStr.substr(q1 + 1, q2 - q1 - 1);
                size_t colon = bindingsStr.find(':', q2);
                if (colon == std::string::npos) break;
                // Get the array
                size_t arrStart = bindingsStr.find('[', colon);
                size_t arrEnd   = bindingsStr.find(']', arrStart);
                if (arrStart == std::string::npos || arrEnd == std::string::npos) break;
                std::string arrContent = bindingsStr.substr(arrStart + 1,
                                                             arrEnd - arrStart - 1);
                std::vector<int> nodeIds;
                std::istringstream ss(arrContent);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    size_t s = tok.find_first_not_of(" \t\n\r");
                    if (s == std::string::npos) continue;
                    tok = tok.substr(s);
                    try { nodeIds.push_back(std::stoi(tok)); } catch(...) {}
                }
                if (!varName.empty()) {
                    state.bindings[varName] = nodeIds;
                }
                pos = arrEnd + 1;
            }
        }
    }
    return state;
}

// Extract the "state" sub-object from an event JSON
static std::string extractStateJson(const std::string& eventJson) {
    size_t kp = eventJson.find("\"state\"");
    if (kp == std::string::npos) return "";
    size_t brace = eventJson.find('{', kp);
    if (brace == std::string::npos) return "";
    int depth = 0;
    for (size_t i = brace; i < eventJson.size(); i++) {
        if (eventJson[i] == '{') depth++;
        else if (eventJson[i] == '}') { depth--; if (depth == 0) return eventJson.substr(brace, i - brace + 1); }
    }
    return "";
}

// ============================================================
// Load runtime events from JSON
// ============================================================
std::vector<ConcreteEvent> CorrelationEngine::loadRuntimeEvents(
        const std::string& path) {
    std::string json = readFile(path);
    if (json.empty()) {
        std::cerr << "[CorrelationEngine] WARNING: Cannot read runtime events: "
                  << path << "\n";
        return {};
    }
    std::vector<ConcreteEvent> result;
    auto objs = splitJsonArray(json, "events");
    for (const auto& obj : objs) {
        ConcreteEvent ev;
        ev.type         = jsonGetString(obj, "type");
        ev.stmt_id      = jsonGetInt(obj, "stmt_id");
        ev.line         = jsonGetInt(obj, "stmt_line");
        ev.func_name    = jsonGetString(obj, "func_name");
        ev.var_name     = jsonGetString(obj, "var_name");
        ev.field_name   = jsonGetString(obj, "field_name");
        ev.addr         = jsonGetString(obj, "addr");
        ev.addr2        = jsonGetString(obj, "addr2");
        std::string sz  = jsonGetString(obj, "size");
        try { ev.size   = std::stoull(sz); } catch(...) { ev.size = 0; }
        ev.alloc_site   = jsonGetInt(obj, "alloc_site");
        ev.branch_taken = jsonGetInt(obj, "branch_taken", -1);
        ev.loop_iter    = jsonGetInt(obj, "loop_iter");
        result.push_back(ev);
    }
    return result;
}

// ============================================================
// Load static trace from JSON
// ============================================================
std::vector<StaticEvent> CorrelationEngine::loadStaticTrace(
        const std::string& path,
        int& worklist_iters, int& transfers,
        int& pruned, int& mde_hits, int& mde_misses) {
    std::string json = readFile(path);
    if (json.empty()) {
        std::cerr << "[CorrelationEngine] WARNING: Cannot read static trace: "
                  << path << "\n";
        return {};
    }

    // Read stats
    size_t statsKey = json.find("\"stats\"");
    if (statsKey != std::string::npos) {
        size_t brace = json.find('{', statsKey);
        size_t end   = json.find('}', brace);
        if (brace != std::string::npos && end != std::string::npos) {
            std::string statsJson = json.substr(brace, end - brace + 1);
            worklist_iters = jsonGetInt(statsJson, "worklistIterations");
            transfers      = jsonGetInt(statsJson, "transfers");
            pruned         = jsonGetInt(statsJson, "statementsPruned");
            mde_hits       = jsonGetInt(statsJson, "mdeHits");
            mde_misses     = jsonGetInt(statsJson, "mdeMisses");
        }
    }

    std::vector<StaticEvent> result;
    auto objs = splitJsonArray(json, "trace");
    // Keep track of previous state for before/after
    std::optional<StaticHRAState> prevState;
    for (const auto& obj : objs) {
        StaticEvent ev;
        ev.type     = jsonGetString(obj, "type");
        ev.block_id = jsonGetInt(obj, "blockId", -1);
        ev.stmt     = jsonGetString(obj, "stmt");
        ev.pruned   = jsonGetBool(obj, "pruned");
        ev.is_mde_new = jsonGetBool(obj, "isNew");
        ev.mde_index  = jsonGetInt(obj, "mdeIndex", -1);
        ev.is_mde_hit = (obj.find("\"isNew\": false") != std::string::npos ||
                          obj.find("\"isNew\":false") != std::string::npos);
        ev.worklist_queue = jsonGetIntArray(obj, "queue");

        std::string stateJson = extractStateJson(obj);
        if (!stateJson.empty()) {
            ev.state_before = prevState;
            ev.state_after  = parseHRAState(stateJson);
            prevState       = ev.state_after;
        }
        result.push_back(ev);
    }
    return result;
}

// ============================================================
// Build alloc site map from concrete events
// ============================================================
static std::map<std::string, AllocCorrelation> buildAllocMap(
        const std::vector<ConcreteEvent>& concrete) {
    std::map<std::string, AllocCorrelation> m;
    for (const auto& ev : concrete) {
        if (ev.type == "RT_MALLOC" && !ev.addr.empty() && ev.alloc_site > 0) {
            AllocCorrelation ac;
            ac.concrete_addr = ev.addr;
            ac.alloc_site    = ev.alloc_site;
            ac.abstract_node = "N" + std::to_string(ev.alloc_site);
            m[ev.addr] = ac;
        }
    }
    return m;
}

// Map static event type + stmt to operation string
static std::string mapOperation(const StaticEvent& ev, const ConcreteEvent& cev) {
    if (!cev.type.empty()) {
        if (cev.type == "RT_PROGRAM_START")  return "program_start";
        if (cev.type == "RT_PROGRAM_END")    return "program_end";
        if (cev.type == "RT_FUNCTION_ENTER") return "function_enter";
        if (cev.type == "RT_FUNCTION_EXIT")  return "function_exit";
        if (cev.type == "RT_MALLOC")         return "alloc";
        if (cev.type == "RT_FIELD_LOAD")     return "field_load";
        if (cev.type == "RT_FIELD_STORE")    return "field_store";
        if (cev.type == "RT_PTR_ASSIGN")     return "assign";
        if (cev.type == "RT_BRANCH")         return "branch";
        if (cev.type == "RT_LOOP_ITER")      return "loop_iter";
        if (cev.type == "RT_RETURN")         return "return";
    }
    if (!ev.stmt.empty()) {
        if (ev.stmt.find("alloc") != std::string::npos) return "alloc";
        size_t arrowPos = ev.stmt.find("->");
        size_t eqPos    = ev.stmt.find("=");
        if (arrowPos != std::string::npos && eqPos != std::string::npos) {
            if (arrowPos < eqPos) return "field_store";
            else return "field_load";
        }
        if (ev.stmt.find("=") != std::string::npos)  return "assign";
        if (ev.stmt.find("return") != std::string::npos) return "return";
    }
    return "unknown";
}

// ============================================================
// Main correlate function
// ============================================================
CorrelatedTrace CorrelationEngine::correlate(
        const std::string& runtimeJsonPath,
        const std::string& traceA,
        const std::string& traceB,
        const std::string& traceC) {

    CorrelatedTrace result;

    // Load all event streams
    auto concrete = loadRuntimeEvents(runtimeJsonPath);
    result.all_concrete = concrete;

    int unused_mde_h, unused_mde_m;
    if (!traceA.empty())
        result.all_static_A = loadStaticTrace(traceA,
            result.static_worklist_iters_A, result.static_transfers_A,
            result.static_pruned_B, unused_mde_h, unused_mde_m);
    if (!traceB.empty())
        result.all_static_B = loadStaticTrace(traceB,
            result.static_worklist_iters_B, result.static_transfers_B,
            result.static_pruned_B, unused_mde_h, unused_mde_m);
    if (!traceC.empty())
        result.all_static_C = loadStaticTrace(traceC,
            result.static_worklist_iters_C, result.static_transfers_C,
            result.static_pruned_C, result.mde_hits, result.mde_misses);

    // Build alloc site map (visualization only)
    result.alloc_site_map = buildAllocMap(concrete);

    // Compute runtime statistics
    for (const auto& ev : concrete) {
        if (ev.type == "RT_MALLOC")    result.runtime_malloc_count++;
        if (ev.type == "RT_BRANCH")    result.runtime_branch_count++;
        if (ev.type == "RT_LOOP_ITER") result.runtime_loop_iterations =
                                           std::max(result.runtime_loop_iterations,
                                                    ev.loop_iter);
    }

    // Precise static matching
    auto matchEvent = [](const StaticEvent& sev, const ConcreteEvent& cev) -> bool {
        if (sev.type != "StmtPost") return false;
        if (sev.stmt.empty()) return false;
        if (cev.type == "RT_MALLOC") {
            return (sev.stmt.find("alloc#" + std::to_string(cev.alloc_site)) != std::string::npos);
        }
        if (cev.type == "RT_FIELD_STORE") {
            size_t arrowPos = sev.stmt.find("->");
            size_t eqPos    = sev.stmt.find("=");
            if (arrowPos == std::string::npos || eqPos == std::string::npos) return false;
            if (arrowPos >= eqPos) return false; // In store: base->field = val
            if (!cev.field_name.empty() && sev.stmt.find("->" + cev.field_name) == std::string::npos) {
                return false;
            }
            return true;
        }
        if (cev.type == "RT_FIELD_LOAD") {
            size_t arrowPos = sev.stmt.find("->");
            size_t eqPos    = sev.stmt.find("=");
            if (arrowPos == std::string::npos || eqPos == std::string::npos) return false;
            if (eqPos >= arrowPos) return false; // In load: dst = base->field
            if (!cev.field_name.empty() && sev.stmt.find("->" + cev.field_name) == std::string::npos) {
                return false;
            }
            if (!cev.var_name.empty() && sev.stmt.find(cev.var_name) == std::string::npos) {
                return false;
            }
            return true;
        }
        if (cev.type == "RT_PTR_ASSIGN") {
            if (sev.stmt.find("alloc") != std::string::npos) return false;
            if (sev.stmt.find("->") != std::string::npos) return false;
            if (sev.stmt.find("=") == std::string::npos) return false;
            return (!cev.var_name.empty() && sev.stmt.find(cev.var_name) != std::string::npos);
        }
        if (cev.type == "RT_RETURN") {
            return (sev.stmt.find("return") != std::string::npos);
        }
        return false;
    };

    size_t cursorA = 0;
    size_t cursorB = 0;
    size_t cursorC = 0;
    std::set<size_t> matchedIndicesA;
    std::set<size_t> matchedIndicesB;
    std::set<size_t> matchedIndicesC;

    // Track dynamic occurrence indices per (line, type)
    std::map<std::pair<int, std::string>, int> occCounts;

    // Process ALL concrete events (preserving repeated dynamic events in loops)
    for (const auto& cev : concrete) {
        int occIdx = ++occCounts[{cev.line, cev.type}];

        CorrelatedEvent ce;
        ce.line             = cev.line;
        ce.stmt_id          = cev.stmt_id;
        ce.occurrence_index = occIdx;
        ce.has_concrete     = true;
        ce.concrete         = cev;

        // Classify lifecycle and control-flow events
        bool isLifecycle = (cev.type == "RT_PROGRAM_START" ||
                            cev.type == "RT_PROGRAM_END" ||
                            cev.type == "RT_FUNCTION_ENTER" ||
                            cev.type == "RT_FUNCTION_EXIT");
        bool isControlFlow = (cev.type == "RT_BRANCH" || cev.type == "RT_LOOP_ITER");

        if (isLifecycle || isControlFlow) {
            ce.correlation_status = CorrelationStatus::NOT_APPLICABLE_TO_STATIC_ANALYSIS;
            ce.is_concrete_only   = true;
            result.summary.not_applicable_count++;
            result.summary.concrete_only_count++;
            result.concrete_only_events.push_back(cev);
            ce.operation = mapOperation(StaticEvent{}, cev);
            result.events.push_back(ce);
            continue;
        }

        // Find next matching static event in A
        for (size_t i = cursorA; i < result.all_static_A.size(); i++) {
            if (matchEvent(result.all_static_A[i], cev)) {
                ce.has_static_A = true;
                ce.static_A     = result.all_static_A[i];
                cursorA         = i + 1;
                matchedIndicesA.insert(i);
                break;
            }
        }
        // Find next matching static event in B
        for (size_t i = cursorB; i < result.all_static_B.size(); i++) {
            if (matchEvent(result.all_static_B[i], cev)) {
                ce.has_static_B = true;
                ce.static_B     = result.all_static_B[i];
                cursorB         = i + 1;
                matchedIndicesB.insert(i);
                break;
            }
        }
        // Find next matching static event in C
        for (size_t i = cursorC; i < result.all_static_C.size(); i++) {
            if (matchEvent(result.all_static_C[i], cev)) {
                ce.has_static_C = true;
                ce.static_C     = result.all_static_C[i];
                cursorC         = i + 1;
                matchedIndicesC.insert(i);
                break;
            }
        }

        if (ce.has_static_A || ce.has_static_B || ce.has_static_C) {
            ce.correlation_status = CorrelationStatus::MATCHED;
            result.summary.matched_count++;
        } else {
            ce.correlation_status = CorrelationStatus::UNMATCHED;
            ce.is_concrete_only   = true;
            result.summary.unmatched_count++;
            result.summary.concrete_only_count++;
            result.concrete_only_events.push_back(cev);
        }

        // Alloc site correlation
        if (!cev.addr.empty() && result.alloc_site_map.count(cev.addr)) {
            ce.has_alloc_correlation = true;
            ce.alloc_corr = result.alloc_site_map.at(cev.addr);
        }

        ce.operation = mapOperation(ce.has_static_A ? ce.static_A : (ce.has_static_C ? ce.static_C : StaticEvent{}), cev);
        result.events.push_back(ce);
    }

    // Identify static-only StmtPost events that had no corresponding concrete execution
    for (size_t k = 0; k < result.all_static_C.size(); k++) {
        const auto& sev = result.all_static_C[k];
        if (sev.type == "StmtPost" && !matchedIndicesC.count(k)) {
            result.static_only_events.push_back(sev);
            result.summary.static_only_count++;

            CorrelatedEvent ce;
            ce.has_concrete       = false;
            ce.is_static_only     = true;
            ce.has_static_C       = true;
            ce.static_C           = sev;
            ce.correlation_status = CorrelationStatus::UNMATCHED;
            ce.operation          = mapOperation(sev, ConcreteEvent{});
            result.events.push_back(ce);
        }
    }

    return result;
}

// ============================================================
// Write correlated JSON output
// ============================================================
static void writeStateJson(std::ofstream& f, const StaticHRAState& s) {
    f << "{\"bindings\":{";
    bool first = true;
    for (const auto& [var, nodes] : s.bindings) {
        if (!first) { f << ","; }
        first = false;
        f << "\"" << var << "\":[";
        for (size_t i = 0; i < nodes.size(); i++) {
            if (i) { f << ","; }
            f << nodes[i];
        }
        f << "]";
    }
    f << "},\"nodes\":[";
    for (size_t i = 0; i < s.nodes.size(); i++) {
        if (i) { f << ","; }
        f << s.nodes[i];
    }
    f << "],\"edges\":[";
    for (size_t i = 0; i < s.edges.size(); i++) {
        if (i) { f << ","; }
        f << "{\"source\":" << s.edges[i].source
          << ",\"field\":\"" << s.edges[i].field << "\""
          << ",\"target\":" << s.edges[i].target << "}";
    }
    f << "]}";
}

static void writeStaticEvent(std::ofstream& f, const StaticEvent& ev, bool has) {
    if (!has) { f << "null"; return; }
    f << "{\"type\":\"" << ev.type << "\""
      << ",\"stmt\":\"" << ev.stmt << "\""
      << ",\"pruned\":" << (ev.pruned ? "true" : "false")
      << ",\"mde_hit\":" << (ev.is_mde_hit ? "true" : "false")
      << ",\"mde_index\":" << ev.mde_index;
    if (ev.state_after) {
        f << ",\"state\":"; writeStateJson(f, *ev.state_after);
    } else {
        f << ",\"state\":null";
    }
    f << "}";
}

bool CorrelationEngine::writeCorrelatedJSON(const CorrelatedTrace& trace,
                                             const std::string& outputPath) {
    std::ofstream f(outputPath);
    if (!f.is_open()) {
        std::cerr << "[CorrelationEngine] ERROR: Cannot write to " << outputPath << "\n";
        return false;
    }

    f << "{\n"
      << "  \"domain_note\": \"CONCRETE_RUNTIME and STATIC_ANALYSIS are separate domains. "
         "Concrete addresses != abstract NodeIDs. "
         "alloc_site_map is visualization-only and does not modify HRA semantics.\",\n"
      << "  \"benchmark\": \"" << trace.benchmark_name << "\",\n"
      << "  \"summary\": {\n"
      << "    \"matched_count\": " << trace.summary.matched_count << ",\n"
      << "    \"unmatched_count\": " << trace.summary.unmatched_count << ",\n"
      << "    \"ambiguous_count\": " << trace.summary.ambiguous_count << ",\n"
      << "    \"not_applicable_count\": " << trace.summary.not_applicable_count << ",\n"
      << "    \"concrete_only_count\": " << trace.summary.concrete_only_count << ",\n"
      << "    \"static_only_count\": " << trace.summary.static_only_count << "\n"
      << "  },\n"
      << "  \"runtime_stats\": {\n"
      << "    \"domain\": \"CONCRETE_RUNTIME\",\n"
      << "    \"loop_iterations\": " << trace.runtime_loop_iterations << ",\n"
      << "    \"malloc_count\": " << trace.runtime_malloc_count << ",\n"
      << "    \"branch_count\": " << trace.runtime_branch_count << "\n"
      << "  },\n"
      << "  \"static_stats\": {\n"
      << "    \"domain\": \"STATIC_ANALYSIS\",\n"
      << "    \"worklist_iters_A\": " << trace.static_worklist_iters_A << ",\n"
      << "    \"worklist_iters_B\": " << trace.static_worklist_iters_B << ",\n"
      << "    \"worklist_iters_C\": " << trace.static_worklist_iters_C << ",\n"
      << "    \"transfers_A\": " << trace.static_transfers_A << ",\n"
      << "    \"transfers_B\": " << trace.static_transfers_B << ",\n"
      << "    \"transfers_C\": " << trace.static_transfers_C << ",\n"
      << "    \"pruned_B\": " << trace.static_pruned_B << ",\n"
      << "    \"pruned_C\": " << trace.static_pruned_C << ",\n"
      << "    \"mde_hits\": " << trace.mde_hits << ",\n"
      << "    \"mde_misses\": " << trace.mde_misses << "\n"
      << "  },\n"
      << "  \"alloc_site_map\": [\n";

    bool firstAlloc = true;
    for (const auto& [addr, ac] : trace.alloc_site_map) {
        if (!firstAlloc) { f << ",\n"; }
        firstAlloc = false;
        f << "    {\"concrete_addr\":\"" << ac.concrete_addr << "\""
          << ",\"alloc_site\":" << ac.alloc_site
          << ",\"abstract_node\":\"" << ac.abstract_node
          << "\",\"visualization_only\":true}";
    }
    f << "\n  ],\n";

    // All concrete events
    f << "  \"all_concrete\": [\n";
    for (size_t i = 0; i < trace.all_concrete.size(); i++) {
        const auto& ev = trace.all_concrete[i];
        if (i) f << ",\n";
        f << "    {\"type\":\"" << ev.type << "\""
          << ",\"stmt_id\":" << ev.stmt_id
          << ",\"line\":" << ev.line
          << ",\"func_name\":\"" << ev.func_name << "\""
          << ",\"var_name\":\"" << ev.var_name << "\""
          << ",\"field_name\":\"" << ev.field_name << "\""
          << ",\"addr\":\"" << ev.addr << "\""
          << ",\"addr2\":\"" << ev.addr2 << "\""
          << ",\"size\":" << ev.size
          << ",\"alloc_site\":" << ev.alloc_site
          << ",\"branch_taken\":" << ev.branch_taken
          << ",\"loop_iter\":" << ev.loop_iter << "}";
    }
    f << "\n  ],\n";

    // Concrete only events
    f << "  \"concrete_only\": [\n";
    for (size_t i = 0; i < trace.concrete_only_events.size(); i++) {
        const auto& ev = trace.concrete_only_events[i];
        if (i) f << ",\n";
        f << "    {\"type\":\"" << ev.type << "\""
          << ",\"stmt_id\":" << ev.stmt_id
          << ",\"line\":" << ev.line
          << ",\"var_name\":\"" << ev.var_name << "\""
          << ",\"field_name\":\"" << ev.field_name << "\""
          << ",\"addr\":\"" << ev.addr << "\""
          << ",\"addr2\":\"" << ev.addr2 << "\""
          << ",\"alloc_site\":" << ev.alloc_site << "}";
    }
    f << "\n  ],\n";

    // Static only events
    f << "  \"static_only\": [\n";
    for (size_t i = 0; i < trace.static_only_events.size(); i++) {
        if (i) f << ",\n";
        writeStaticEvent(f, trace.static_only_events[i], true);
    }
    f << "\n  ],\n";

    // Correlated events
    f << "  \"correlated_events\": [\n";
    for (size_t i = 0; i < trace.events.size(); i++) {
        const auto& ce = trace.events[i];
        if (i) f << ",\n";
        f << "    {\n"
          << "      \"line\": " << ce.line << ",\n"
          << "      \"stmt_id\": " << ce.stmt_id << ",\n"
          << "      \"occurrence_index\": " << ce.occurrence_index << ",\n"
          << "      \"operation\": \"" << ce.operation << "\",\n"
          << "      \"correlation_status\": \"" << ce.correlation_status << "\",\n"
          << "      \"is_concrete_only\": " << (ce.is_concrete_only ? "true" : "false") << ",\n"
          << "      \"is_static_only\": " << (ce.is_static_only ? "true" : "false") << ",\n"
          << "      \"concrete\": ";
        if (ce.has_concrete) {
            const auto& ev = ce.concrete;
            f << "{\"type\":\"" << ev.type << "\""
              << ",\"var\":\"" << ev.var_name << "\""
              << ",\"addr\":\"" << ev.addr << "\""
              << ",\"addr2\":\"" << ev.addr2 << "\""
              << ",\"field\":\"" << ev.field_name << "\""
              << ",\"alloc_site\":" << ev.alloc_site
              << ",\"branch_taken\":" << ev.branch_taken
              << ",\"loop_iter\":" << ev.loop_iter << "}";
        } else {
            f << "null";
        }
        f << ",\n      \"static_A\": "; writeStaticEvent(f, ce.static_A, ce.has_static_A);
        f << ",\n      \"static_B\": "; writeStaticEvent(f, ce.static_B, ce.has_static_B);
        f << ",\n      \"static_C\": "; writeStaticEvent(f, ce.static_C, ce.has_static_C);
        if (ce.has_alloc_correlation) {
            f << ",\n      \"alloc_correlation\": {"
              << "\"concrete_addr\":\"" << ce.alloc_corr.concrete_addr << "\""
              << ",\"alloc_site\":" << ce.alloc_corr.alloc_site
              << ",\"abstract_node\":\"" << ce.alloc_corr.abstract_node << "\""
              << ",\"visualization_only\":true}";
        }
        f << "\n    }";
    }
    f << "\n  ]\n}\n";

    std::cerr << "[CorrelationEngine] Written: " << outputPath << "\n";
    return true;
}

// Implement interface stubs
StaticHRAState CorrelationEngine::parseHRAState(const std::string& json) {
    return ::phase14::parseHRAState(json);
}
std::string CorrelationEngine::simpleJsonGet(const std::string& json,
                                              const std::string& key) {
    return jsonGetString(json, key);
}
std::vector<std::string> CorrelationEngine::simpleJsonArray(
        const std::string& json, const std::string& key) {
    return jsonGetStringArray(json, key);
}

}  // namespace phase14
