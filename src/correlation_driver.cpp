/**
 * correlation_driver.cpp
 *
 * Command-line driver for Phase 14:
 * Concrete Runtime <-> Static Analysis Correlation Engine
 *
 * Reads:
 *   --runtime <path>   Concrete runtime events JSON (runtime_events.json)
 *   --trace-a <path>   Static analysis Config A trace JSON (optional)
 *   --trace-b <path>   Static analysis Config B trace JSON (optional)
 *   --trace-c <path>   Static analysis Config C trace JSON (optional)
 *   --output  <path>   Output correlated trace JSON
 *   --benchmark <name> Benchmark identifier (e.g., BM01, BM08)
 *
 * Produces unified correlated trace JSON for visualization and analysis.
 */

#include "mdehra/runtime/CorrelationEngine.hpp"
#include <iostream>
#include <string>

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << "\n"
              << "  --runtime <runtime_events.json>\n"
              << "  --output  <correlated_trace.json>\n"
              << "  [--trace-a <trace_A.json>]\n"
              << "  [--trace-b <trace_B.json>]\n"
              << "  [--trace-c <trace_C.json>]\n"
              << "  [--benchmark <name>]\n";
}

int main(int argc, char** argv) {
    std::string runtimePath;
    std::string traceA;
    std::string traceB;
    std::string traceC;
    std::string outputPath = "correlated_trace.json";
    std::string benchmarkName = "benchmark";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--runtime" && i + 1 < argc) {
            runtimePath = argv[++i];
        } else if (arg == "--trace-a" && i + 1 < argc) {
            traceA = argv[++i];
        } else if (arg == "--trace-b" && i + 1 < argc) {
            traceB = argv[++i];
        } else if (arg == "--trace-c" && i + 1 < argc) {
            traceC = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--benchmark" && i + 1 < argc) {
            benchmarkName = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (runtimePath.empty()) {
        std::cerr << "Error: --runtime path is required.\n";
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "[CorrelationDriver] Correlating execution for: " << benchmarkName << "\n"
              << "  Concrete runtime : " << runtimePath << "\n"
              << "  Static Config A  : " << (traceA.empty() ? "(none)" : traceA) << "\n"
              << "  Static Config B  : " << (traceB.empty() ? "(none)" : traceB) << "\n"
              << "  Static Config C  : " << (traceC.empty() ? "(none)" : traceC) << "\n";

    phase14::CorrelatedTrace trace = phase14::CorrelationEngine::correlate(
        runtimePath, traceA, traceB, traceC);

    trace.benchmark_name = benchmarkName;

    std::cout << "[CorrelationDriver] Loaded:\n"
              << "  Concrete events   : " << trace.all_concrete.size() << "\n"
              << "  Static A events   : " << trace.all_static_A.size() << "\n"
              << "  Static B events   : " << trace.all_static_B.size() << "\n"
              << "  Static C events   : " << trace.all_static_C.size() << "\n"
              << "  Correlated points : " << trace.events.size() << "\n"
              << "  Alloc site mappings: " << trace.alloc_site_map.size() << "\n";

    if (!phase14::CorrelationEngine::writeCorrelatedJSON(trace, outputPath)) {
        std::cerr << "[CorrelationDriver] Failed to write correlated JSON.\n";
        return 1;
    }

    std::cout << "[CorrelationDriver] Successfully generated " << outputPath << "\n";
    return 0;
}
