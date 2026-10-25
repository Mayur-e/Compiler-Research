/**
 * CorrelationEngine.hpp
 *
 * Phase 14: Concrete Runtime ↔ Static Analysis Correlation Engine
 *
 * Reads:
 *   - runtime_events.json  (concrete execution, domain: CONCRETE RUNTIME)
 *   - *_A_trace.json       (static analysis Config A, domain: STATIC)
 *   - *_B_trace.json       (static analysis Config B, domain: STATIC)
 *   - *_C_trace.json       (static analysis Config C, domain: STATIC)
 *
 * Correlates events by source line number (primary key).
 *
 * Produces:
 *   - A sequence of CorrelatedEvent objects, each containing:
 *       - The concrete runtime event (if any) at this line/point
 *       - The matching static analysis state from A/B/C (if any)
 *
 * DOMAIN SEPARATION CONTRACT:
 *   - Concrete addresses (e.g., 0x1000) are NEVER placed in static fields.
 *   - Abstract NodeIDs (e.g., N1) are NEVER placed in concrete fields.
 *   - The alloc_site_map links: concrete addr → alloc site → abstract ID.
 *     This mapping is for VISUALIZATION ONLY and does not modify HRA.
 *
 * IMPORTANT: "Runtime loop iterations" and "Static worklist iterations"
 *   are DIFFERENT quantities and are NEVER summed or compared directly.
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace phase14 {

// ============================================================
// Concrete domain types (RUNTIME only)
// ============================================================

struct ConcreteEvent {
    std::string  type;          // RT_MALLOC, RT_FIELD_LOAD, etc.
    int          stmt_id = 0;
    int          line    = 0;
    std::string  func_name;
    std::string  var_name;
    std::string  field_name;
    std::string  addr;          // hex string "0x1000"
    std::string  addr2;         // hex string
    std::size_t  size    = 0;
    int          alloc_site = 0;
    int          branch_taken = -1;  // -1 = not applicable
    int          loop_iter    = 0;

    bool isNull(const std::string& a) const { return a == "0x0"; }
    bool hasAddr() const { return !addr.empty() && addr != "0x0"; }
};

// Maps concrete address → (alloc_site, abstract_node_label)
// e.g., "0x1000" → {1, "N1"}
// VISUALIZATION ONLY — does not modify HRA semantics.
struct AllocCorrelation {
    std::string  concrete_addr;
    int          alloc_site;
    std::string  abstract_node;  // "N1", "N2", etc.
};

// ============================================================
// Static analysis domain types
// ============================================================

struct StaticStateBinding {
    std::string var;
    std::vector<int> nodes;  // abstract NodeIDs
};

struct StaticStateEdge {
    int         source;
    std::string field;
    int         target;
};

struct StaticHRAState {
    std::map<std::string, std::vector<int>> bindings;  // var → [NodeIDs]
    std::vector<int>                        nodes;
    std::vector<StaticStateEdge>            edges;
    bool                                    empty() const {
        return bindings.empty() && nodes.empty() && edges.empty();
    }
};

struct StaticLivenessInfo {
    std::vector<std::string> use;
    std::vector<std::string> def;
    std::vector<std::string> live_before;
    std::vector<std::string> live_after;
};

struct StaticEvent {
    std::string          type;       // BlockPop, StmtPre, StmtPost, Canonicalize, etc.
    int                  block_id = -1;
    std::string          stmt;       // IR statement text
    bool                 pruned   = false;
    bool                 is_mde_hit  = false;
    bool                 is_mde_new  = false;
    int                  mde_index   = -1;
    std::optional<StaticHRAState> state_before;
    std::optional<StaticHRAState> state_after;
    std::optional<StaticLivenessInfo> liveness;
    std::vector<int>     worklist_queue;
};

// ============================================================
// ============================================================
// Correlation Status
// ============================================================
namespace CorrelationStatus {
    inline const std::string MATCHED                           = "MATCHED";
    inline const std::string UNMATCHED                         = "UNMATCHED";
    inline const std::string AMBIGUOUS                         = "AMBIGUOUS";
    inline const std::string NOT_APPLICABLE_TO_STATIC_ANALYSIS = "NOT_APPLICABLE_TO_STATIC_ANALYSIS";
}

struct CorrelationSummary {
    int matched_count        = 0;
    int unmatched_count      = 0;
    int ambiguous_count      = 0;
    int not_applicable_count = 0;
    int concrete_only_count  = 0;
    int static_only_count    = 0;
};

// ============================================================
// Correlated event: one source line / statement / dynamic occurrence
// ============================================================

struct CorrelatedEvent {
    // Source identity (correlation key)
    int         line             = 0;
    int         stmt_id          = 0;
    int         occurrence_index = 1;
    std::string operation;   // "alloc", "assign", "field_load", "field_store",
                             //  "branch", "loop_iter", "return", "program_start", etc.
    std::string correlation_status = "UNMATCHED";
    bool        is_concrete_only   = false;
    bool        is_static_only     = false;

    // CONCRETE RUNTIME domain
    bool                           has_concrete = false;
    ConcreteEvent                  concrete;

    // STATIC ANALYSIS domain — three configurations
    bool                           has_static_A = false;
    bool                           has_static_B = false;
    bool                           has_static_C = false;
    StaticEvent                    static_A;
    StaticEvent                    static_B;
    StaticEvent                    static_C;

    // Allocation site correlation (visualization only)
    bool                           has_alloc_correlation = false;
    AllocCorrelation               alloc_corr;
};

// ============================================================
// Full correlated trace
// ============================================================

struct CorrelatedTrace {
    std::string  benchmark_name;
    std::string  source_file;

    // Correlation summary statistics
    CorrelationSummary summary;

    // Concrete domain statistics
    int  runtime_loop_iterations  = 0;  // real loop iters observed at runtime
    int  runtime_malloc_count     = 0;
    int  runtime_branch_count     = 0;

    // Static domain statistics (separate, never summed with runtime)
    int  static_worklist_iters_A  = 0;  // worklist iterations per config
    int  static_worklist_iters_B  = 0;
    int  static_worklist_iters_C  = 0;
    int  static_transfers_A       = 0;
    int  static_transfers_B       = 0;
    int  static_transfers_C       = 0;
    int  static_pruned_B          = 0;
    int  static_pruned_C          = 0;
    int  mde_hits                 = 0;
    int  mde_misses               = 0;

    // Allocation site map (visualization only)
    std::map<std::string, AllocCorrelation> alloc_site_map;  // addr → corr

    // The correlated event stream (preserving dynamic occurrences)
    std::vector<CorrelatedEvent> events;

    // Separate concrete-only and static-only event lists
    std::vector<ConcreteEvent>   concrete_only_events;
    std::vector<StaticEvent>     static_only_events;

    // Raw data for panels that need it
    std::vector<ConcreteEvent>   all_concrete;
    std::vector<StaticEvent>     all_static_A;
    std::vector<StaticEvent>     all_static_B;
    std::vector<StaticEvent>     all_static_C;
};

// ============================================================
// Correlation Engine
// ============================================================

class CorrelationEngine {
public:
    /**
     * Load and correlate all event streams.
     *
     * @param runtimeJsonPath  Path to runtime_events.json (concrete domain)
     * @param traceA           Path to *_A_trace.json (static Config A)
     * @param traceB           Path to *_B_trace.json (static Config B)
     * @param traceC           Path to *_C_trace.json (static Config C)
     * @return                 Populated CorrelatedTrace
     *
     * If any trace file is missing/empty, that domain is left unpopulated
     * and the relevant has_static_X flags remain false.
     */
    static CorrelatedTrace correlate(
        const std::string& runtimeJsonPath,
        const std::string& traceA,
        const std::string& traceB,
        const std::string& traceC);

    /**
     * Write the correlated trace to a JSON file for the visualizer.
     */
    static bool writeCorrelatedJSON(
        const CorrelatedTrace& trace,
        const std::string& outputPath);

private:
    static std::vector<ConcreteEvent> loadRuntimeEvents(
        const std::string& path);

    static std::vector<StaticEvent> loadStaticTrace(
        const std::string& path,
        int& worklist_iters, int& transfers, int& pruned,
        int& mde_hits, int& mde_misses);

    static StaticHRAState parseHRAState(const std::string& json);
    static std::string   simpleJsonGet(const std::string& json,
                                        const std::string& key);
    static std::vector<std::string> simpleJsonArray(const std::string& json,
                                                     const std::string& key);
};

}  // namespace phase14
