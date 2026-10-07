#include "mdehra/ir/Program.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

using namespace mdehra;

size_t getPeakMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
#endif
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "HEADER") {
        std::cout << "program,configuration,run_id,success,runtime_ms,frontend_ms,liveness_ms,hra_ms,peak_mem_bytes,"
                  << "states_constructed,entry_states,exit_states,blocks_processed,worklist_iters,"
                  << "transfers,allocs,assigns,loads,stores,"
                  << "stmts_examined,stmts_pruned,bindings_removed,"
                  << "canon_calls,canon_new,canon_reused,"
                  << "liveness_iters,liveness_blocks,"
                  << "mde_state_hits,mde_state_misses\n";
        return 0;
    }

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <source_file> <config_type> [run_id]\n";
        std::cerr << "       " << argv[0] << " HEADER\n";
        std::cerr << "config_type: A, B, or C\n";
        return 1;
    }
    
    std::string sourceFile = argv[1];
    std::string configType = argv[2];
    std::string runId = (argc > 3) ? argv[3] : "1";

    std::ifstream ifs(sourceFile);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open " << sourceFile << "\n";
        return 1;
    }
    
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string fullCode = "void* __builtin_malloc(unsigned long long);\nstruct Node { struct Node* next; struct Node* prev; int data; };\n" + code;
    
    auto ast = clang::tooling::buildASTFromCode(fullCode);
    if (!ast) {
        std::cerr << "Failed to parse code.\n";
        return 1;
    }
    
    auto t0 = std::chrono::high_resolution_clock::now();
    auto program = frontend::ClangFrontend::translate(ast->getASTContext());
    auto t1 = std::chrono::high_resolution_clock::now();
    auto frontendTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (program->functions.empty()) {
        std::cerr << "No functions found.\n";
        return 1;
    }

    const ir::Function* funcToTest = nullptr;
    for (const auto& kv : program->functions) {
        if (!kv.second.blocks.empty()) {
            funcToTest = &kv.second;
            break;
        }
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    liveness::LivenessSolver ls;
    liveness::LivenessResult livenessRes;
    
    if (configType == "B" || configType == "C") {
        livenessRes = ls.run(*funcToTest);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    auto livenessTime = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

    auto t4 = std::chrono::high_resolution_clock::now();
    
    solver::BaselineHRAConfig configA;
    solver::LivenessGuidedHRAConfig configB(&livenessRes);
    mde_adapter::MDEGuidedHRAConfig configC(&livenessRes);
    
    solver::HRAConfig* currentConfig = nullptr;
    if (configType == "A") currentConfig = &configA;
    else if (configType == "B") currentConfig = &configB;
    else if (configType == "C") currentConfig = &configC;
    else {
        std::cerr << "Invalid config type\n";
        return 1;
    }

    solver::HRASolver solver;
    solver.run(*funcToTest, currentConfig);
    
    auto t5 = std::chrono::high_resolution_clock::now();
    auto hraTime = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count();

    size_t peakMem = getPeakMemory();
    
    solver::HRASolverStats stats = currentConfig->getStats();
    
    // MDE Metrics
    size_t mdeHits = 0;
    size_t mdeMisses = 0;
    
    size_t edgeHits = 0;
    size_t edgeMisses = 0;
    size_t graphHits = 0;
    size_t graphMisses = 0;
    size_t bindingHits = 0;
    size_t bindingMisses = 0;

    if (configType == "C") {
        mdeHits = configC.mdeHits;
        mdeMisses = configC.mdeMisses;
        
        /* 
        // Perf map access
        const auto& targetNodesPerf = configC.encoder.targetNodes.get_perf();
        const auto& edgeSetsPerf = configC.encoder.edgeSets.get_perf();
        const auto& bindingsPerf = configC.encoder.bindings.get_perf();
        const auto& graphsPerf = configC.encoder.graphs.get_perf();
        
        auto getHits = [](const auto& perfMap) -> size_t {
            if (perfMap.find("property_sets") != perfMap.end()) return perfMap.at("property_sets").hits;
            return 0;
        };
        auto getMisses = [](const auto& perfMap) -> size_t {
            if (perfMap.find("property_sets") != perfMap.end()) return perfMap.at("property_sets").cold_misses;
            return 0;
        };
        */
    }

    // Print CSV row
    std::cout << sourceFile << "," << configType << "," << runId << ",1,"
              << (frontendTime + livenessTime + hraTime) << ","
              << frontendTime << "," << livenessTime << "," << hraTime << "," << peakMem << ","
              << stats.statesConstructed << "," << stats.entryStatesProduced << "," << stats.exitStatesProduced << ","
              << stats.blocksProcessed << "," << stats.worklistIterations << ","
              << stats.transfersExecuted << "," << stats.allocsProcessed << "," << stats.assignsProcessed << "," << stats.loadsProcessed << "," << stats.storesProcessed << ","
              << stats.statementsExamined << "," << stats.statementsPruned << "," << stats.bindingsRemoved << ","
              << stats.canonicalizeCalls << "," << stats.newCanonicalStates << "," << stats.reusedCanonicalStates << ","
              << livenessRes.stats.iterations << "," << livenessRes.stats.blocksProcessed << ","
              << mdeHits << "," << mdeMisses << "\n";

    return 0;
}
