// =============================================================
// performance_analyzer.cpp  (Phase 15)
//
// Native C++ performance comparison tool for MDE-HRA.
//
// Usage:
//   perf-analyzer <source.c> [options]
//   Options:
//     --reps N        Number of measured repetitions (default: 10)
//     --warmup W      Warm-up runs before measurement (default: 1)
//     --json <path>   Write machine-readable results to path
//     --no-runtime    Skip per-rep analysis (just run once)
//     --explain       Print theoretical complexity discussion
//
// Measures:
//   - Wall-clock time for frontend, liveness, and each A/B/C solver
//   - Peak Working Set memory via Win32 GetProcessMemoryInfo (KB)
//   - Worklist iterations, transfer ops, peak worklist/heap sizes
//   - MDE hits, misses, canonical state counts
//   - CanonicalResult(B) == CanonicalResult(C) correctness invariant
//
// Does NOT execute benchmark source code concretely.
// Does NOT use MDE cache hits as a memory or correctness proxy.
// =============================================================

#include "mdehra/ir/Program.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/solver/CanonicalResult.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include "clang/Tooling/Tooling.h"

#include <windows.h>
#include <psapi.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <set>
#include <cstring>
#include <cmath>

using namespace mdehra;
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// ---------------------------------------------------------------
// Peak Working Set (KB) via Win32 PSAPI
// On Windows/MSYS2 this is the most reliable per-process peak
// resident memory measurement.  Note: this is process-wide peak
// (includes LLVM/Clang library memory), not solver-only.
// ---------------------------------------------------------------
static size_t getPeakWorkingSetKB() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize / 1024;
    }
    return 0;
}

// ---------------------------------------------------------------
// Per-configuration result
// ---------------------------------------------------------------
struct ConfigResult {
    std::string config;           // "A", "B", or "C"
    double      analysisTimeMs  = 0;  // median over reps
    double      analysisTimeP25 = 0;
    double      analysisTimeP75 = 0;
    size_t      peakMemoryKB    = 0;  // process peak WS (measured once)
    size_t      worklistIters   = 0;
    size_t      transfers       = 0;
    size_t      allocsProcessed = 0;
    size_t      assignsProcessed= 0;
    size_t      loadsProcessed  = 0;
    size_t      storesProcessed = 0;
    size_t      peakWorklistSz  = 0;
    size_t      peakHeapNodes   = 0;
    size_t      finalHeapNodes  = 0;
    size_t      stmtsExamined   = 0;
    size_t      stmtsPruned     = 0;
    size_t      bindingsRemoved = 0;
    size_t      mdeHits         = 0;  // C only
    size_t      mdeMisses       = 0;  // C only
    size_t      mdeUnique       = 0;  // C only
    std::string canonicalStr;
    bool        canonOk         = false;  // BC_Match
    int         exitStatus      = 0;
};

// ---------------------------------------------------------------
// Run all three configurations once, returning stats
// ---------------------------------------------------------------
struct OneRun {
    solver::HRASolverStats statsA, statsB, statsC;
    int mdeHitsC = 0, mdeMissesC = 0, mdeUniqueC = 0;
    double timeA = 0, timeB = 0, timeC = 0;
    double timeFrontend = 0, timeLiveness = 0;
    std::string canonA, canonB, canonC;
    bool bcMatch = false;
    size_t finalNodesA = 0, finalNodesB = 0, finalNodesC = 0;
};

static OneRun runOnce(const std::string& code) {
    OneRun r;

    // --- Frontend ---
    auto t0 = Clock::now();
    auto ast = clang::tooling::buildASTFromCode(code);
    r.timeFrontend = Ms(Clock::now() - t0).count();

    if (!ast) return r;
    auto program = frontend::ClangFrontend::translate(ast->getASTContext());
    if (!program || program->functions.empty()) return r;

    const ir::Function* func = nullptr;
    for (const auto& kv : program->functions) {
        if (!kv.second.blocks.empty()) { func = &kv.second; break; }
    }
    if (!func) return r;

    // Find exit block
    ir::ID exitBlockId = 0;
    for (const auto& kv : func->blocks) {
        if (kv.second.successors.empty()) { exitBlockId = kv.first; break; }
    }

    // --- Liveness ---
    auto tL = Clock::now();
    liveness::LivenessSolver ls;
    auto livenessRes = ls.run(*func);
    r.timeLiveness = Ms(Clock::now() - tL).count();

    std::set<ir::Variable> liveAtExit;
    if (livenessRes.liveOut.count(exitBlockId))
        liveAtExit = livenessRes.liveOut[exitBlockId];

    // --- Config A ---
    {
        auto tA = Clock::now();
        solver::BaselineHRAConfig cfgA;
        solver::HRASolver solverA;
        solverA.run(*func, &cfgA);
        r.timeA = Ms(Clock::now() - tA).count();
        r.statsA = cfgA.getStats();
        // finalHeapNodes: exit state
        try {
            auto es = solverA.getBlockExitState(exitBlockId);
            r.finalNodesA = es ? es->graph.nodes.size() : 0;
            r.canonA = solver::getCanonicalOutput(es, liveAtExit);
        } catch(...) {}
    }

    // --- Config B ---
    {
        auto tB = Clock::now();
        solver::LivenessGuidedHRAConfig cfgB(&livenessRes);
        solver::HRASolver solverB;
        solverB.run(*func, &cfgB);
        r.timeB = Ms(Clock::now() - tB).count();
        r.statsB = cfgB.getStats();
        try {
            auto es = solverB.getBlockExitState(exitBlockId);
            r.finalNodesB = es ? es->graph.nodes.size() : 0;
            r.canonB = solver::getCanonicalOutput(es, liveAtExit);
        } catch(...) {}
    }

    // --- Config C ---
    {
        auto tC = Clock::now();
        mde_adapter::MDEGuidedHRAConfig cfgC(&livenessRes);
        solver::HRASolver solverC;
        solverC.run(*func, &cfgC);
        r.timeC = Ms(Clock::now() - tC).count();
        r.statsC = cfgC.getStats();
        r.mdeHitsC   = cfgC.mdeHits;
        r.mdeMissesC = cfgC.mdeMisses;
        // Each miss creates a new canonical state; misses == unique states seen
        r.mdeUniqueC = cfgC.mdeMisses;
        try {
            auto es = solverC.getBlockExitState(exitBlockId);
            r.finalNodesC = es ? es->graph.nodes.size() : 0;
            r.canonC = solver::getCanonicalOutput(es, liveAtExit);
        } catch(...) {}
    }

    r.bcMatch = (r.canonB == r.canonC);
    return r;
}

// ---------------------------------------------------------------
// Median helper
// ---------------------------------------------------------------
static double median(std::vector<double>& v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 0) ? (v[n/2-1] + v[n/2]) / 2.0 : v[n/2];
}
static double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    double idx = p * (v.size() - 1);
    size_t lo = (size_t)idx;
    double frac = idx - lo;
    if (lo + 1 >= v.size()) return v.back();
    return v[lo] * (1 - frac) + v[lo+1] * frac;
}

// ---------------------------------------------------------------
// Aligned table helpers
// ---------------------------------------------------------------
static void printSep(char c = '-', int w = 65) {
    std::cout << std::string(w, c) << '\n';
}
static void printRow(const std::string& label, const std::string& a,
                     const std::string& b, const std::string& c,
                     int labelW = 28, int colW = 11) {
    std::cout << std::left  << std::setw(labelW) << label
              << std::right << std::setw(colW)   << a
              << std::setw(colW)   << b
              << std::setw(colW)   << c
              << '\n';
}

// ---------------------------------------------------------------
// Complexity explanation
// ---------------------------------------------------------------
static void printComplexityExplanation(const OneRun& r) {
    std::cout << "\n";
    printSep('=', 65);
    std::cout << "  TIME & SPACE COMPLEXITY ANALYSIS\n";
    printSep('=', 65);
    std::cout << R"(
Theoretical Background
----------------------
Let:
  F   = number of functions (1 in the demo)
  B   = number of CFG basic blocks
  E   = number of CFG edges
  S   = number of IR statements
  P   = number of pointer variables
  N   = number of allocation sites (abstract heap nodes)
  W   = maximum worklist length
  I   = total worklist iterations to fixpoint

HRA Transfer Function — per statement:
  Alloc/Assign: O(1)
  Load:  O(P * E_graph)  — scan edges for each source node
  Store: O(P^2)          — all (base, value) pairs
Overall transfer cost per iteration: O(S * P^2 * N)

Worklist Fixpoint Convergence:
  Worst case: O(I * B * S * P^2 * N)
  In practice: far fewer iterations because most states stabilize
  early and the worklist is small.

State Representation:
  Bindings map: O(P) entries, each pointing to a set of NodeIDs.
  Heap graph:   O(N + E_graph) for nodes and edges.
  MDE state key (Config C): structural hash O(P + N + E_graph).

Config A (Baseline HRA):
  No pruning. All statements are transferred. State space is O(P*N).
  Memory: O(B * P * N) for stored entry/exit states.

Config B (Liveness + HRA):
  Dead-variable pruning reduces effective P to live pointer count.
  Fewer bindings propagated across blocks → smaller states.
  Asymptotically same worst case but typically O(P_live * N) states.

Config C (Liveness + HRA + MDE):
  Same analysis as B; MDE adds O(P_live + N + E_graph) hashing per
  canonicalizeState call. Returns a cached shared_ptr on cache hit
  → fixpoint detection via pointer equality is O(1) instead of O(P*N).
  MDE cache size: O(number of distinct reachable abstract states).

Measured Observations (this run, single rep)
-------------------------------------------
)" ;
    std::cout << "  Worklist iterations A=" << r.statsA.worklistIterations
              << "  B=" << r.statsB.worklistIterations
              << "  C=" << r.statsC.worklistIterations << "\n";
    std::cout << "  Transfer ops       A=" << r.statsA.transfersExecuted
              << "  B=" << r.statsB.transfersExecuted
              << "  C=" << r.statsC.transfersExecuted << "\n";
    std::cout << "  Peak heap nodes    A=" << r.statsA.peakHeapNodes
              << "  B=" << r.statsB.peakHeapNodes
              << "  C=" << r.statsC.peakHeapNodes << "\n";
    std::cout << "  Stmts pruned       A=" << r.statsA.statementsPruned
              << "  B=" << r.statsB.statementsPruned
              << "  C=" << r.statsC.statementsPruned << "\n";
    std::cout << "  MDE cache hits                            C=" << r.mdeHitsC << "\n";
    std::cout << "\nIMPORTANT CAVEATS\n";
    std::cout << "-----------------\n";
    std::cout << "  * Peak memory is process-wide (includes LLVM/Clang library);\n";
    std::cout << "    it is NOT solver-only memory.  Solver state counts (heap\n";
    std::cout << "    nodes, worklist entries) are the closest solver-only metric.\n";
    std::cout << "  * A finite benchmark campaign does NOT prove an asymptotic\n";
    std::cout << "    speedup.  All Big-O bounds above are worst-case theoretical\n";
    std::cout << "    and may not be tight for the specific input structures.\n";
    std::cout << "  * MDE cache hits reduce redundant state-equality checks but\n";
    std::cout << "    do NOT change the semantic analysis result.\n";
    std::cout << "  * Config B may produce different bindings than A for\n";
    std::cout << "    dead variables; the canonical comparison uses live-out only.\n";
    printSep('-', 65);
}

// ---------------------------------------------------------------
// JSON export
// ---------------------------------------------------------------
static void writeJSON(const std::string& path,
                      const std::string& benchmark,
                      const std::vector<ConfigResult>& results,
                      const std::vector<OneRun>& reps,
                      const std::string& buildInfo) {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "[-] Cannot open JSON output: " << path << "\n";
        return;
    }

    f << "{\n";
    f << "  \"benchmark\": \"" << benchmark << "\",\n";
    f << "  \"build_info\": \"" << buildInfo << "\",\n";
    f << "  \"summary\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        f << "    {\n";
        f << "      \"config\": \""            << r.config << "\",\n";
        f << "      \"analysis_time_ms_median\": " << std::fixed << std::setprecision(3) << r.analysisTimeMs << ",\n";
        f << "      \"analysis_time_ms_p25\": "    << r.analysisTimeP25 << ",\n";
        f << "      \"analysis_time_ms_p75\": "    << r.analysisTimeP75 << ",\n";
        f << "      \"peak_memory_kb\": "          << r.peakMemoryKB << ",\n";
        f << "      \"worklist_iterations\": "     << r.worklistIters << ",\n";
        f << "      \"transfers_executed\": "      << r.transfers << ",\n";
        f << "      \"allocs_processed\": "        << r.allocsProcessed << ",\n";
        f << "      \"assigns_processed\": "       << r.assignsProcessed << ",\n";
        f << "      \"loads_processed\": "         << r.loadsProcessed << ",\n";
        f << "      \"stores_processed\": "        << r.storesProcessed << ",\n";
        f << "      \"peak_worklist_size\": "      << r.peakWorklistSz << ",\n";
        f << "      \"peak_heap_nodes\": "         << r.peakHeapNodes << ",\n";
        f << "      \"final_heap_nodes\": "        << r.finalHeapNodes << ",\n";
        f << "      \"stmts_examined\": "          << r.stmtsExamined << ",\n";
        f << "      \"stmts_pruned\": "            << r.stmtsPruned << ",\n";
        f << "      \"bindings_removed\": "        << r.bindingsRemoved << ",\n";
        f << "      \"mde_hits\": "                << r.mdeHits << ",\n";
        f << "      \"mde_misses\": "              << r.mdeMisses << ",\n";
        f << "      \"mde_unique_states\": "       << r.mdeUnique << ",\n";
        f << "      \"bc_match\": "                << (r.canonOk ? "true" : "false") << "\n";
        f << "    }" << (i+1 < results.size() ? "," : "") << "\n";
    }
    f << "  ],\n";
    f << "  \"raw_reps\": [\n";
    for (size_t i = 0; i < reps.size(); ++i) {
        const auto& rr = reps[i];
        f << "    {\"rep\": " << i
          << ", \"time_A_ms\": " << std::fixed << std::setprecision(3) << rr.timeA
          << ", \"time_B_ms\": " << rr.timeB
          << ", \"time_C_ms\": " << rr.timeC
          << ", \"bc_match\": "  << (rr.bcMatch ? "true" : "false")
          << "}" << (i+1 < reps.size() ? "," : "") << "\n";
    }
    f << "  ],\n";
    f << "  \"bc_invariant\": " << (results.size() >= 3 ? (results[2].canonOk ? "true" : "false") : "false") << "\n";
    f << "}\n";
    std::cout << "[+] JSON results written to: " << path << "\n";
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: perf-analyzer <source.c> [--reps N] [--warmup W] [--json path] [--explain]\n";
        return 1;
    }

    std::string sourceFile;
    int  reps    = 10;
    int  warmup  = 1;
    bool explain = false;
    std::string jsonPath;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--reps") == 0 && i+1 < argc)        reps    = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--warmup") == 0 && i+1 < argc) warmup  = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--json") == 0 && i+1 < argc)   jsonPath = argv[++i];
        else if (std::strcmp(argv[i], "--explain") == 0)               explain  = true;
        else sourceFile = argv[i];
    }

    if (sourceFile.empty()) {
        std::cerr << "[-] No source file specified.\n";
        return 1;
    }

    std::ifstream ifs(sourceFile);
    if (!ifs.is_open()) {
        std::cerr << "[-] Cannot open: " << sourceFile << "\n";
        return 1;
    }
    std::string srcCode((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    // Standard preamble matching trace_driver / e2e tests
    std::string fullCode =
        "void* __builtin_malloc(unsigned long long);\n"
        "struct Node { struct Node* next; struct Node* prev; int data; };\n"
        + srcCode;

    // Extract benchmark name from path
    std::string benchmark = sourceFile;
    {
        size_t pos = benchmark.find_last_of("/\\");
        if (pos != std::string::npos) benchmark = benchmark.substr(pos+1);
        pos = benchmark.rfind('.');
        if (pos != std::string::npos) benchmark = benchmark.substr(0, pos);
    }

    // Header
    std::cout << "\n";
    printSep('=', 65);
    std::cout << "       MDE-HRA PERFORMANCE COMPARISON\n";
    printSep('=', 65);
    std::cout << "Benchmark: " << benchmark << "\n";
    std::cout << "Source:    " << sourceFile << "\n";
    std::cout << "Reps:      " << reps << "  Warmup: " << warmup << "\n";
    printSep('-', 65);

    // --- Warm-up ---
    std::cout << "[*] Warming up (" << warmup << " run(s))...\n";
    for (int w = 0; w < warmup; ++w) runOnce(fullCode);

    // --- Measured reps ---
    std::cout << "[*] Running " << reps << " measurement repetition(s)...\n";
    std::vector<OneRun> measured;
    measured.reserve(reps);
    for (int rep = 0; rep < reps; ++rep) {
        measured.push_back(runOnce(fullCode));
    }

    // Collect time series per config
    std::vector<double> timesA, timesB, timesC;
    for (auto& rr : measured) {
        timesA.push_back(rr.timeA);
        timesB.push_back(rr.timeB);
        timesC.push_back(rr.timeC);
    }

    // Use last rep for counters (stable after warm-up)
    const OneRun& last = measured.back();

    // Peak WS memory (process-wide, measured once after all reps)
    size_t peakKB = getPeakWorkingSetKB();

    // Build ConfigResult summaries
    auto makeResult = [&](const std::string& cfg,
                          std::vector<double>& times,
                          const solver::HRASolverStats& stats,
                          size_t finalNodes,
                          int mdeH, int mdeM, int mdeU,
                          const std::string& canon) -> ConfigResult {
        ConfigResult cr;
        cr.config         = cfg;
        cr.analysisTimeMs = median(times);
        cr.analysisTimeP25= percentile(times, 0.25);
        cr.analysisTimeP75= percentile(times, 0.75);
        cr.peakMemoryKB   = peakKB;          // process-wide
        cr.worklistIters  = stats.worklistIterations;
        cr.transfers      = stats.transfersExecuted;
        cr.allocsProcessed= stats.allocsProcessed;
        cr.assignsProcessed=stats.assignsProcessed;
        cr.loadsProcessed = stats.loadsProcessed;
        cr.storesProcessed= stats.storesProcessed;
        cr.peakWorklistSz = stats.peakWorklistSize;
        cr.peakHeapNodes  = stats.peakHeapNodes;
        cr.finalHeapNodes = finalNodes;
        cr.stmtsExamined  = stats.statementsExamined;
        cr.stmtsPruned    = stats.statementsPruned;
        cr.bindingsRemoved= stats.bindingsRemoved;
        cr.mdeHits        = (size_t)mdeH;
        cr.mdeMisses      = (size_t)mdeM;
        cr.mdeUnique      = (size_t)mdeU;
        cr.canonicalStr   = canon;
        cr.canonOk        = (cfg != "C") || (last.bcMatch);
        return cr;
    };

    ConfigResult rA = makeResult("A", timesA, last.statsA, last.finalNodesA, 0, 0, 0, last.canonA);
    ConfigResult rB = makeResult("B", timesB, last.statsB, last.finalNodesB, 0, 0, 0, last.canonB);
    ConfigResult rC = makeResult("C", timesC, last.statsC, last.finalNodesC,
                                 last.mdeHitsC, last.mdeMissesC, last.mdeUniqueC, last.canonC);
    rC.canonOk = last.bcMatch;

    bool bcOk = last.bcMatch;

    // ---- Print table ----
    std::cout << "\n";
    printRow("Metric", "A", "B", "C");
    printSep('-', 65);

    auto fmtMs = [](double ms) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << ms;
        return ss.str();
    };
    auto fmtN = [](size_t n) { return std::to_string(n); };
    auto fmtNA = [](bool cond, size_t n) -> std::string {
        return cond ? std::to_string(n) : "N/A";
    };

    printRow("Analysis time (ms) [med]",   fmtMs(rA.analysisTimeMs), fmtMs(rB.analysisTimeMs), fmtMs(rC.analysisTimeMs));
    printRow("  P25–P75 (ms)",
             "["+fmtMs(rA.analysisTimeP25)+"–"+fmtMs(rA.analysisTimeP75)+"]",
             "["+fmtMs(rB.analysisTimeP25)+"–"+fmtMs(rB.analysisTimeP75)+"]",
             "["+fmtMs(rC.analysisTimeP25)+"–"+fmtMs(rC.analysisTimeP75)+"]");
    printRow("Peak memory (KB) [proc WS]", fmtN(rA.peakMemoryKB), fmtN(rB.peakMemoryKB), fmtN(rC.peakMemoryKB));
    printSep('.', 65);
    printRow("Worklist iterations",  fmtN(rA.worklistIters), fmtN(rB.worklistIters), fmtN(rC.worklistIters));
    printRow("Transfer ops",         fmtN(rA.transfers),     fmtN(rB.transfers),     fmtN(rC.transfers));
    printRow("  allocs",             fmtN(rA.allocsProcessed),  fmtN(rB.allocsProcessed),  fmtN(rC.allocsProcessed));
    printRow("  assigns",            fmtN(rA.assignsProcessed), fmtN(rB.assignsProcessed), fmtN(rC.assignsProcessed));
    printRow("  loads",              fmtN(rA.loadsProcessed),   fmtN(rB.loadsProcessed),   fmtN(rC.loadsProcessed));
    printRow("  stores",             fmtN(rA.storesProcessed),  fmtN(rB.storesProcessed),  fmtN(rC.storesProcessed));
    printRow("Peak worklist size",   fmtN(rA.peakWorklistSz), fmtN(rB.peakWorklistSz), fmtN(rC.peakWorklistSz));
    printRow("Peak abstract heap nodes", fmtN(rA.peakHeapNodes), fmtN(rB.peakHeapNodes), fmtN(rC.peakHeapNodes));
    printRow("Final heap nodes",     fmtN(rA.finalHeapNodes), fmtN(rB.finalHeapNodes), fmtN(rC.finalHeapNodes));
    printRow("Stmts examined",       fmtN(rA.stmtsExamined),  fmtN(rB.stmtsExamined),  fmtN(rC.stmtsExamined));
    printRow("Stmts pruned",         fmtN(rA.stmtsPruned),    fmtN(rB.stmtsPruned),    fmtN(rC.stmtsPruned));
    printRow("Bindings removed",     fmtN(rA.bindingsRemoved),fmtN(rB.bindingsRemoved),fmtN(rC.bindingsRemoved));
    printSep('.', 65);
    printRow("MDE cache hits",   "N/A", "N/A", fmtN(rC.mdeHits));
    printRow("MDE cache misses", "N/A", "N/A", fmtN(rC.mdeMisses));
    printRow("MDE unique states","N/A", "N/A", fmtN(rC.mdeUnique));
    printSep('.', 65);

    // Canonical result status
    auto canonStatus = [](const std::string& canon) -> std::string {
        if (canon.empty()) return "N/A";
        // short summary: first binding line
        size_t nl = canon.find('\n');
        std::string first = (nl != std::string::npos) ? canon.substr(0, nl) : canon;
        if (first.size() > 22) first = first.substr(0, 19) + "...";
        return first;
    };
    printRow("Canonical result",  canonStatus(rA.canonicalStr),
                                  canonStatus(rB.canonicalStr),
                                  canonStatus(rC.canonicalStr));

    printSep('-', 65);

    // Improvement rows
    auto pctStr = [](double base, double cmp) -> std::string {
        if (base <= 0) return "N/A";
        double pct = 100.0 * (base - cmp) / base;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        if (pct >= 0) ss << "+" << pct << "% faster";
        else          ss << std::abs(pct) << "% slower";
        return ss.str();
    };

    std::cout << "A vs B time improvement:   " << pctStr(rA.analysisTimeMs, rB.analysisTimeMs) << "\n";
    std::cout << "A vs C time improvement:   " << pctStr(rA.analysisTimeMs, rC.analysisTimeMs) << "\n";
    std::cout << "B vs C time difference:    " << pctStr(rB.analysisTimeMs, rC.analysisTimeMs) << "\n";
    if (rA.worklistIters > 0) {
        std::cout << "A vs B worklist reduction: "
                  << pctStr((double)rA.worklistIters, (double)rB.worklistIters) << "\n";
        std::cout << "A vs C worklist reduction: "
                  << pctStr((double)rA.worklistIters, (double)rC.worklistIters) << "\n";
    }

    printSep('-', 65);
    std::cout << "B/C canonical-result equality: " << (bcOk ? "PASS (B == C)" : "FAIL (B != C)") << "\n";
    if (!bcOk) {
        std::cout << "  [!] B canonical:\n" << last.canonB << "\n";
        std::cout << "  [!] C canonical:\n" << last.canonC << "\n";
    }

    std::cout << "\nMEMORY NOTE:\n";
    std::cout << "  Peak memory is process-wide peak Working Set (PSAPI).\n";
    std::cout << "  It includes the LLVM/Clang frontend (~50–200 MB).\n";
    std::cout << "  Solver-only space is approximated by heap-node and\n";
    std::cout << "  worklist-entry counts above.\n";
    printSep('=', 65);

    // --- Complexity explanation ---
    if (explain) {
        printComplexityExplanation(last);
    }

    // --- JSON export ---
    std::vector<ConfigResult> allResults = {rA, rB, rC};
    if (!jsonPath.empty()) {
        writeJSON(jsonPath, benchmark, allResults, measured, "MSYS2-UCRT64-MinGW");
    }

    return bcOk ? 0 : 2;
}
