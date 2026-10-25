// ===========================================================================
// MDE-HRA Driver (mdehra-driver)
//
// Research-grade command-line interface for:
// MDE-HRA: Canonical State Sharing and Liveness-Guided Pruning in Heap Reference Analysis
//
// Configurations:
//   A: Baseline HRA (no liveness, no MDE canonicalization)
//   B: Liveness + HRA (liveness-guided dead reference pruning)
//   C: Liveness + HRA + MDE (full proposed system with structural deduplication)
//   D: HRA + MDE (ablation: MDE structural deduplication without liveness)
//
// Exit Codes:
//   0: Analysis completed successfully (OK)
//   2: Frontend rejected unsupported constructs in strict mode (UNSUPPORTED)
//   1: Input/runtime error or analysis exception (ANALYSIS_ERROR)
// ===========================================================================

#include "mdehra/config.hpp"
#include "mdehra/ir/Program.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/solver/CanonicalResult.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include "clang/Tooling/Tooling.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <filesystem>

using namespace mdehra;
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// ---------------------------------------------------------------------------
// Self-contained SHA-256 implementation for deterministic source hashing
// ---------------------------------------------------------------------------
namespace sha256_detail {
    inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    inline uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    inline uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    inline std::string compute(const std::string& input) {
        uint64_t totalBits = (uint64_t)input.size() * 8;
        std::vector<uint8_t> msg(input.begin(), input.end());
        msg.push_back(0x80);
        while ((msg.size() % 64) != 56) msg.push_back(0x00);
        for (int i = 7; i >= 0; --i) msg.push_back((uint8_t)((totalBits >> (i * 8)) & 0xff));

        uint32_t H[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            uint32_t W[64];
            for (int t = 0; t < 16; ++t) {
                W[t] = ((uint32_t)msg[chunk + t*4] << 24) |
                       ((uint32_t)msg[chunk + t*4 + 1] << 16) |
                       ((uint32_t)msg[chunk + t*4 + 2] << 8) |
                       ((uint32_t)msg[chunk + t*4 + 3]);
            }
            for (int t = 16; t < 64; ++t) {
                W[t] = theta1(W[t - 2]) + W[t - 7] + theta0(W[t - 15]) + W[t - 16];
            }
            uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            for (int t = 0; t < 64; ++t) {
                uint32_t T1 = h + sig1(e) + ch(e, f, g) + K[t] + W[t];
                uint32_t T2 = sig0(a) + maj(a, b, c);
                h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
            }
            H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
        }

        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(8) << H[i];
        }
        return ss.str();
    }
}

// ---------------------------------------------------------------------------
// Peak Working Set (KB)
// ---------------------------------------------------------------------------
static size_t getPeakWorkingSetKB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize / 1024;
    }
#endif
    return 0;
}

// ---------------------------------------------------------------------------
// Options & CLI Arguments
// ---------------------------------------------------------------------------
struct DriverOptions {
    std::string inputFile;
    std::string selectedConfig = "ALL"; // "A", "B", "C", "D", or "ALL"
    bool strictMode = false;
    bool permissiveMode = false;
    std::string jsonOutput;
    int repetitions = 1;
    int warmup = 0;
    bool explain = false;
    bool showVersion = false;
    bool showHelp = false;
};

static void printHelp(const char* exe) {
    std::cout << "Usage: " << exe << " [options] <source.c>\n\n"
              << "Options:\n"
              << "  --input <path>          Path to source C file\n"
              << "  --config <A|B|C|D|ALL>  Analysis configuration (default: ALL)\n"
              << "                            A: Baseline HRA\n"
              << "                            B: Liveness + HRA\n"
              << "                            C: Liveness + HRA + MDE\n"
              << "                            D: HRA + MDE (no liveness)\n"
              << "  --strict                Strict frontend mode (reject unsupported constructs)\n"
              << "  --permissive            Permissive mode (allow incomplete analysis)\n"
              << "  --reps <N>              Repetitions for timing measurement (default: 1)\n"
              << "  --warmup <W>            Warmup iterations (default: 0)\n"
              << "  --json <path>           Emit machine-readable JSON results\n"
              << "  --explain               Print theoretical explanation and details\n"
              << "  --version               Print version information\n"
              << "  --help                  Show this help message\n";
}

static DriverOptions parseArgs(int argc, char** argv) {
    DriverOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            opts.showHelp = true;
        } else if (arg == "--version" || arg == "-v") {
            opts.showVersion = true;
        } else if (arg == "--strict") {
            opts.strictMode = true;
        } else if (arg == "--permissive") {
            opts.permissiveMode = true;
        } else if (arg == "--explain") {
            opts.explain = true;
        } else if (arg == "--config" && i + 1 < argc) {
            opts.selectedConfig = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            opts.inputFile = argv[++i];
        } else if (arg == "--json" && i + 1 < argc) {
            opts.jsonOutput = argv[++i];
        } else if (arg == "--reps" && i + 1 < argc) {
            opts.repetitions = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--warmup" && i + 1 < argc) {
            opts.warmup = std::max(0, std::atoi(argv[++i]));
        } else if (arg.rfind("--", 0) != 0 && opts.inputFile.empty()) {
            opts.inputFile = arg;
        }
    }
    return opts;
}

// ---------------------------------------------------------------------------
// Per-config execution result
// ---------------------------------------------------------------------------
struct SingleConfigResult {
    std::string configLabel;
    std::string status = "OK";
    double timeTotalMs = 0.0;
    double timeFrontendMs = 0.0;
    double timeLivenessMs = 0.0;
    double timeSolverMs = 0.0;
    double timeCanonicalizeMs = 0.0;
    size_t peakRssKB = 0;
    size_t retainedBytes = 0;
    size_t worklistIters = 0;
    size_t transfers = 0;
    size_t allocs = 0;
    size_t assigns = 0;
    size_t loads = 0;
    size_t stores = 0;
    size_t peakWorklist = 0;
    size_t peakHeapNodes = 0;
    size_t finalHeapNodes = 0;
    size_t stmtsExamined = 0;
    size_t stmtsPruned = 0;
    size_t bindingsRemoved = 0;
    size_t mdeCalls = 0;
    size_t mdeHits = 0;
    size_t mdeMisses = 0;
    size_t hashCollisions = 0;
    std::string canonicalResult;
};

static std::string escapeJsonPath(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "/";
        else out += c;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Main Analysis Pipeline
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    DriverOptions opts = parseArgs(argc, argv);

    if (opts.showHelp) {
        printHelp(argv[0]);
        return 0;
    }

    if (opts.showVersion) {
        std::cout << PROJECT_NAME << " " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << "\n"
                  << PROJECT_FULL << "\n"
                  << "Configurations: A (Baseline HRA), B (Liveness+HRA), C (Liveness+HRA+MDE), D (HRA+MDE)\n";
        return 0;
    }

    if (opts.inputFile.empty()) {
        std::cerr << "[-] Error: No input file specified.\n";
        printHelp(argv[0]);
        return 1;
    }

    std::ifstream srcFile(opts.inputFile);
    if (!srcFile.is_open()) {
        std::cerr << "[-] Error: Cannot open input file: " << opts.inputFile << "\n";
        return 1;
    }
    std::string code((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());
    std::string sourceSha256 = sha256_detail::compute(code);

    std::cout << "======================================================================\n"
              << "  MDE-HRA RESEARCH DRIVER: " << opts.inputFile << "\n"
              << "  SHA-256: " << sourceSha256 << "\n"
              << "  Mode: " << (opts.strictMode ? "STRICT" : (opts.permissiveMode ? "PERMISSIVE" : "DEFAULT")) << "\n"
              << "======================================================================\n";

    frontend::FrontendOptions fOpts;
    fOpts.strictMode = opts.strictMode;
    fOpts.trackSourceLocations = true;

    // --- Build Clang AST ---
    auto tAST0 = Clock::now();
    std::vector<std::string> clangArgs = {
        "-w",
        "-isystem", "D:/msys64/ucrt64/lib/clang/22/include",
        "-isystem", "D:/msys64/ucrt64/include"
    };
    std::string parseCode = code;
    if (code.find("struct Node {") == std::string::npos && code.find("struct Node") != std::string::npos) {
        parseCode = "struct Node { struct Node* next; struct Node* prev; void* data; unsigned value; int val; };\n" + code;
    }

    auto ast = clang::tooling::buildASTFromCodeWithArgs(parseCode, clangArgs, opts.inputFile);
    double astTimeMs = Ms(Clock::now() - tAST0).count();

    if (!ast) {
        std::cerr << "[-] Error: Clang failed to parse source code into AST.\n";
        return 1;
    }

    // --- Frontend Translation ---
    auto tFront0 = Clock::now();
    frontend::TranslationResult transRes = frontend::ClangFrontend::translate(ast->getASTContext(), fOpts);
    double frontTimeMs = Ms(Clock::now() - tFront0).count();

    std::cout << "[*] Frontend Translation: " << transRes.status
              << " (" << transRes.diagnostics.size() << " diagnostics, " << frontTimeMs << " ms)\n";

    for (const auto& d : transRes.diagnostics) {
        if (d.severity == frontend::DiagnosticSeverity::Unsupported ||
            d.severity == frontend::DiagnosticSeverity::Error ||
            (d.severity == frontend::DiagnosticSeverity::Warning && opts.explain)) {
            std::cout << "    " << d.toString() << "\n";
        }
    }

    if (!transRes.success) {
        std::cerr << "[-] Analysis rejected in strict mode: unsupported constructs present.\n";
        if (!opts.jsonOutput.empty()) {
            std::filesystem::path p(opts.jsonOutput);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }
            std::ofstream jf(opts.jsonOutput);
            jf << "{\n"
               << "  \"status\": \"UNSUPPORTED\",\n"
               << "  \"source_sha256\": \"" << sourceSha256 << "\",\n"
               << "  \"file\": \"" << escapeJsonPath(opts.inputFile) << "\",\n"
               << "  \"diagnostics_count\": " << transRes.diagnostics.size() << "\n"
               << "}\n";
        }
        return 2;
    }

    if (!transRes.program || transRes.program->functions.empty()) {
        std::cerr << "[-] Error: Translation produced no callable functions.\n";
        return 1;
    }

    const ir::Function* targetFunc = nullptr;
    for (const auto& kv : transRes.program->functions) {
        if (!kv.second.blocks.empty()) { targetFunc = &kv.second; break; }
    }
    if (!targetFunc) {
        std::cerr << "[-] Error: No non-empty function CFG found in program.\n";
        return 1;
    }

    // --- Run Liveness Analysis ---
    auto tLive0 = Clock::now();
    liveness::LivenessSolver liveSolver;
    auto livenessRes = liveSolver.run(*targetFunc);
    double livenessTimeMs = Ms(Clock::now() - tLive0).count();

    // Determine live variables across all exit blocks
    std::set<ir::Variable> liveAtExit;
    for (const auto& kv : targetFunc->blocks) {
        if (kv.second.successors.empty()) {
            auto outIt = livenessRes.liveOut.find(kv.first);
            if (outIt != livenessRes.liveOut.end()) {
                liveAtExit.insert(outIt->second.begin(), outIt->second.end());
            }
        }
    }

    // Determine all variables in the function (for baseline canonicalization A and D)
    std::set<ir::Variable> allVars;
    for (const auto& kv : targetFunc->localVariables) {
        allVars.insert(kv.second);
    }

    std::vector<std::string> configsToRun;
    std::string sel = opts.selectedConfig;
    std::transform(sel.begin(), sel.end(), sel.begin(), ::toupper);
    if (sel == "A" || sel == "B" || sel == "C" || sel == "D") {
        configsToRun.push_back(sel);
    } else {
        configsToRun = {"A", "B", "C", "D"};
    }

    std::map<std::string, SingleConfigResult> results;

    for (const auto& cfgLabel : configsToRun) {
        std::vector<double> timings;
        SingleConfigResult cr;
        cr.configLabel = cfgLabel;

        int totalRuns = opts.warmup + opts.repetitions;
        for (int r = 0; r < totalRuns; ++r) {
            auto tRun0 = Clock::now();

            if (cfgLabel == "A") {
                solver::BaselineHRAConfig cfgA;
                solver::HRASolver sA;
                sA.run(*targetFunc, &cfgA);
                double elapsed = Ms(Clock::now() - tRun0).count();
                if (r >= opts.warmup) timings.push_back(elapsed);
                if (r == totalRuns - 1) {
                    cr.worklistIters = cfgA.stats.worklistIterations;
                    cr.transfers = cfgA.stats.transfersExecuted;
                    cr.allocs = cfgA.stats.allocsProcessed;
                    cr.assigns = cfgA.stats.assignsProcessed;
                    cr.loads = cfgA.stats.loadsProcessed;
                    cr.stores = cfgA.stats.storesProcessed;
                    cr.peakWorklist = cfgA.stats.peakWorklistSize;
                    cr.peakHeapNodes = cfgA.stats.peakHeapNodes;
                    auto es = sA.getFunctionExitState(*targetFunc);
                    cr.finalHeapNodes = es ? es->graph.nodes.size() : 0;
                    cr.canonicalResult = solver::getCanonicalOutput(es, allVars);
                }
            } else if (cfgLabel == "B") {
                solver::LivenessGuidedHRAConfig cfgB(&livenessRes);
                solver::HRASolver sB;
                sB.run(*targetFunc, &cfgB);
                double elapsed = Ms(Clock::now() - tRun0).count();
                if (r >= opts.warmup) timings.push_back(elapsed);
                if (r == totalRuns - 1) {
                    cr.worklistIters = cfgB.stats.worklistIterations;
                    cr.transfers = cfgB.stats.transfersExecuted;
                    cr.allocs = cfgB.stats.allocsProcessed;
                    cr.assigns = cfgB.stats.assignsProcessed;
                    cr.loads = cfgB.stats.loadsProcessed;
                    cr.stores = cfgB.stats.storesProcessed;
                    cr.stmtsExamined = cfgB.stats.statementsExamined;
                    cr.stmtsPruned = cfgB.stats.statementsPruned;
                    cr.bindingsRemoved = cfgB.stats.bindingsRemoved;
                    cr.peakWorklist = cfgB.stats.peakWorklistSize;
                    cr.peakHeapNodes = cfgB.stats.peakHeapNodes;
                    auto es = sB.getFunctionExitState(*targetFunc);
                    cr.finalHeapNodes = es ? es->graph.nodes.size() : 0;
                    cr.canonicalResult = solver::getCanonicalOutput(es, liveAtExit);
                }
            } else if (cfgLabel == "C") {
                mde_adapter::MDEGuidedHRAConfig cfgC(&livenessRes);
                solver::HRASolver sC;
                sC.run(*targetFunc, &cfgC);
                double elapsed = Ms(Clock::now() - tRun0).count();
                if (r >= opts.warmup) timings.push_back(elapsed);
                if (r == totalRuns - 1) {
                    cr.worklistIters = cfgC.stats.worklistIterations;
                    cr.transfers = cfgC.stats.transfersExecuted;
                    cr.allocs = cfgC.stats.allocsProcessed;
                    cr.assigns = cfgC.stats.assignsProcessed;
                    cr.loads = cfgC.stats.loadsProcessed;
                    cr.stores = cfgC.stats.storesProcessed;
                    cr.stmtsExamined = cfgC.stats.statementsExamined;
                    cr.stmtsPruned = cfgC.stats.statementsPruned;
                    cr.bindingsRemoved = cfgC.stats.bindingsRemoved;
                    cr.peakWorklist = cfgC.stats.peakWorklistSize;
                    cr.peakHeapNodes = cfgC.stats.peakHeapNodes;
                    cr.mdeCalls = cfgC.stats.canonicalizeCalls;
                    cr.mdeHits = cfgC.mdeHits;
                    cr.mdeMisses = cfgC.mdeMisses;
                    cr.hashCollisions = cfgC.hashCollisions;
                    cr.timeCanonicalizeMs = cfgC.stats.canonicalizeTimeMs;
                    cr.retainedBytes = cfgC.stats.retainedStateBytes;
                    auto es = sC.getFunctionExitState(*targetFunc);
                    cr.finalHeapNodes = es ? es->graph.nodes.size() : 0;
                    cr.canonicalResult = solver::getCanonicalOutput(es, liveAtExit);
                }
            } else if (cfgLabel == "D") {
                // Configuration D: HRA + MDE without liveness (liveness pointer = nullptr)
                mde_adapter::MDEGuidedHRAConfig cfgD(nullptr);
                solver::HRASolver sD;
                sD.run(*targetFunc, &cfgD);
                double elapsed = Ms(Clock::now() - tRun0).count();
                if (r >= opts.warmup) timings.push_back(elapsed);
                if (r == totalRuns - 1) {
                    cr.worklistIters = cfgD.stats.worklistIterations;
                    cr.transfers = cfgD.stats.transfersExecuted;
                    cr.allocs = cfgD.stats.allocsProcessed;
                    cr.assigns = cfgD.stats.assignsProcessed;
                    cr.loads = cfgD.stats.loadsProcessed;
                    cr.stores = cfgD.stats.storesProcessed;
                    cr.peakWorklist = cfgD.stats.peakWorklistSize;
                    cr.peakHeapNodes = cfgD.stats.peakHeapNodes;
                    cr.mdeCalls = cfgD.stats.canonicalizeCalls;
                    cr.mdeHits = cfgD.mdeHits;
                    cr.mdeMisses = cfgD.mdeMisses;
                    cr.hashCollisions = cfgD.hashCollisions;
                    cr.timeCanonicalizeMs = cfgD.stats.canonicalizeTimeMs;
                    cr.retainedBytes = cfgD.stats.retainedStateBytes;
                    auto es = sD.getFunctionExitState(*targetFunc);
                    cr.finalHeapNodes = es ? es->graph.nodes.size() : 0;
                    cr.canonicalResult = solver::getCanonicalOutput(es, allVars);
                }
            }

            cr.timeFrontendMs = frontTimeMs;
            cr.timeLivenessMs = livenessTimeMs;
            cr.peakRssKB = getPeakWorkingSetKB();
        }

        std::sort(timings.begin(), timings.end());
        cr.timeSolverMs = timings[timings.size() / 2]; // median
        cr.timeTotalMs = cr.timeFrontendMs + (cfgLabel == "B" || cfgLabel == "C" ? cr.timeLivenessMs : 0.0) + cr.timeSolverMs;
        results[cfgLabel] = cr;
    }

    // --- Invariant Checks ---
    bool bcMatch = true;
    bool adMatch = true;
    if (results.count("B") && results.count("C")) {
        bcMatch = (results["B"].canonicalResult == results["C"].canonicalResult);
    }
    if (results.count("A") && results.count("D")) {
        adMatch = (results["A"].canonicalResult == results["D"].canonicalResult);
    }

    // --- Terminal Summary ---
    std::cout << "\n----------------------------------------------------------------------\n"
              << "CFG Config | Solver (ms) | Worklist | Transfers | Pruned | MDE Hit/Miss | Canon Match\n"
              << "----------------------------------------------------------------------\n";
    for (const auto& kv : results) {
        const auto& r = kv.second;
        std::cout << std::left << std::setw(11) << ("Config " + r.configLabel)
                  << " | " << std::fixed << std::setprecision(4) << std::setw(11) << r.timeSolverMs
                  << " | " << std::setw(8) << r.worklistIters
                  << " | " << std::setw(9) << r.transfers
                  << " | " << std::setw(6) << r.stmtsPruned
                  << " | " << r.mdeHits << "/" << r.mdeMisses
                  << "        | " << (r.configLabel == "C" ? (bcMatch ? "BC:PASS" : "BC:FAIL") :
                                     (r.configLabel == "D" ? (adMatch ? "AD:PASS" : "AD:FAIL") : "-"))
                  << "\n";
    }
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << "Invariants: Canonical(B) == Canonical(C): " << (bcMatch ? "PASS" : "FAIL")
              << " | Canonical(A) == Canonical(D): " << (adMatch ? "PASS" : "FAIL") << "\n";

    // --- JSON Output ---
    if (!opts.jsonOutput.empty()) {
        std::filesystem::path p(opts.jsonOutput);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream jf(opts.jsonOutput);
        jf << "{\n"
           << "  \"status\": \"OK\",\n"
           << "  \"source_sha256\": \"" << sourceSha256 << "\",\n"
           << "  \"input_file\": \"" << escapeJsonPath(opts.inputFile) << "\",\n"
           << "  \"function\": \"" << targetFunc->name << "\",\n"
           << "  \"blocks\": " << targetFunc->blocks.size() << ",\n"
           << "  \"bc_invariant\": " << (bcMatch ? "true" : "false") << ",\n"
           << "  \"ad_invariant\": " << (adMatch ? "true" : "false") << ",\n"
           << "  \"peak_rss_kb\": " << getPeakWorkingSetKB() << ",\n"
           << "  \"configs\": {\n";
        bool firstCfg = true;
        for (const auto& kv : results) {
            if (!firstCfg) jf << ",\n";
            firstCfg = false;
            const auto& r = kv.second;
            jf << "    \"" << r.configLabel << "\": {\n"
               << "      \"config\": \"" << r.configLabel << "\",\n"
               << "      \"time_solver_ms\": " << r.timeSolverMs << ",\n"
               << "      \"time_frontend_ms\": " << r.timeFrontendMs << ",\n"
               << "      \"time_liveness_ms\": " << r.timeLivenessMs << ",\n"
               << "      \"time_canonicalize_ms\": " << r.timeCanonicalizeMs << ",\n"
               << "      \"time_total_ms\": " << r.timeTotalMs << ",\n"
               << "      \"worklist_iterations\": " << r.worklistIters << ",\n"
               << "      \"transfers_executed\": " << r.transfers << ",\n"
               << "      \"allocs_processed\": " << r.allocs << ",\n"
               << "      \"assigns_processed\": " << r.assigns << ",\n"
               << "      \"loads_processed\": " << r.loads << ",\n"
               << "      \"stores_processed\": " << r.stores << ",\n"
               << "      \"statements_pruned\": " << r.stmtsPruned << ",\n"
               << "      \"bindings_removed\": " << r.bindingsRemoved << ",\n"
               << "      \"peak_heap_nodes\": " << r.peakHeapNodes << ",\n"
               << "      \"final_heap_nodes\": " << r.finalHeapNodes << ",\n"
               << "      \"mde_calls\": " << r.mdeCalls << ",\n"
               << "      \"mde_hits\": " << r.mdeHits << ",\n"
               << "      \"mde_misses\": " << r.mdeMisses << ",\n"
               << "      \"retained_state_bytes\": " << r.retainedBytes << ",\n"
               << "      \"hash_collisions\": " << r.hashCollisions << "\n"
               << "    }";
        }
        jf << "\n  }\n}\n";
        std::cout << "[+] Wrote results JSON: " << opts.jsonOutput << "\n";
    }

    return 0;
}

