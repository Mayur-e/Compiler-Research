#include "mdehra/ir/Program.hpp"
#include "mdehra/frontend/ClangFrontend.hpp"
#include "mdehra/solver/HRASolver.hpp"
#include "mdehra/solver/HRAConfig.hpp"
#include "mdehra/liveness/LivenessSolver.hpp"
#include "mdehra/mde/MDEGuidedHRAConfig.hpp"
#include "mdehra/ir/Printer.hpp"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

using namespace mdehra;

std::string escapeJSON(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string stateToJSON(const hra::HRAState& state) {
    std::stringstream ss;
    ss << "{";
    ss << "\"bindings\": {";
    bool firstBind = true;
    for (const auto& kv : state.bindings) {
        if (kv.second.empty()) continue;
        if (!firstBind) ss << ", ";
        firstBind = false;
        ss << "\"" << escapeJSON(kv.first.name) << "\": [";
        bool firstNode = true;
        for (auto n : kv.second) {
            if (!firstNode) ss << ", ";
            firstNode = false;
            ss << n;
        }
        ss << "]";
    }
    ss << "}, ";
    ss << "\"nodes\": [";
    bool firstNode2 = true;
    for (auto n : state.graph.nodes) {
        if (!firstNode2) ss << ", ";
        firstNode2 = false;
        ss << n;
    }
    ss << "], ";
    ss << "\"edges\": [";
    bool firstEdge = true;
    for (const auto& e : state.graph.edges) {
        if (!firstEdge) ss << ", ";
        firstEdge = false;
        ss << "{\"source\": " << e.source << ", \"field\": \"" << escapeJSON(e.field.name) << "\", \"target\": " << e.target << "}";
    }
    ss << "]";
    ss << "}";
    return ss.str();
}

std::string stmtToString(const ir::Statement* stmt) {
    std::stringstream ss;
    ir::Printer::print(ss, *stmt);
    return ss.str();
}

template <typename BaseConfig>
class TracingConfig : public BaseConfig {
public:
    template <typename... Args>
    TracingConfig(Args&&... args) : BaseConfig(std::forward<Args>(args)...) {}
    
    std::vector<std::string> events;
    
    void addEvent(const std::string& type, const std::string& data) {
        events.push_back("{\"type\": \"" + type + "\", " + data + "}");
    }

    void traceBlockPop(ir::ID blockId, const std::deque<ir::ID>& worklist) override {
        std::stringstream wl;
        wl << "[";
        for (size_t i = 0; i < worklist.size(); i++) {
            if (i > 0) wl << ", ";
            wl << worklist[i];
        }
        wl << "]";
        addEvent("BlockPop", "\"blockId\": " + std::to_string(blockId) + ", \"queue\": " + wl.str());
    }
    void traceBlockEntry(ir::ID blockId, const hra::HRAState& state) override {
        addEvent("BlockEntry", "\"blockId\": " + std::to_string(blockId) + ", \"state\": " + stateToJSON(state));
    }
    void traceStatementPre(const ir::Statement* stmt, const hra::HRAState& state) override {
        addEvent("StmtPre", "\"stmt\": \"" + escapeJSON(stmtToString(stmt)) + "\", \"state\": " + stateToJSON(state));
    }
    void traceStatementPost(const ir::Statement* stmt, const hra::HRAState& state, bool pruned) override {
        addEvent("StmtPost", "\"stmt\": \"" + escapeJSON(stmtToString(stmt)) + "\", \"state\": " + stateToJSON(state) + ", \"pruned\": " + (pruned ? "true" : "false"));
    }
    void traceBlockExit(ir::ID blockId, const hra::HRAState& state) override {
        addEvent("BlockExit", "\"blockId\": " + std::to_string(blockId) + ", \"state\": " + stateToJSON(state));
    }
    void traceWorklistPush(ir::ID blockId, const std::deque<ir::ID>& worklist) override {
        std::stringstream wl;
        wl << "[";
        for (size_t i = 0; i < worklist.size(); i++) {
            if (i > 0) wl << ", ";
            wl << worklist[i];
        }
        wl << "]";
        addEvent("WorklistPush", "\"blockId\": " + std::to_string(blockId) + ", \"queue\": " + wl.str());
    }
    void traceCanonicalize(const hra::HRAState& original, std::shared_ptr<const hra::HRAState> canonical, bool isNew, size_t mdeIndex) override {
        addEvent("Canonicalize", "\"mdeIndex\": " + std::to_string(mdeIndex) + ", \"isNew\": " + (isNew ? "true" : "false") + ", \"state\": " + stateToJSON(*canonical));
    }
};

std::string cfgToJSON(const ir::Function& func) {
    std::stringstream ss;
    ss << "{";
    bool firstBlock = true;
    for (const auto& kv : func.blocks) {
        if (!firstBlock) ss << ", ";
        firstBlock = false;
        
        ir::ID blockId = kv.first;
        const ir::BasicBlock& b = kv.second;
        
        ss << "\"" << blockId << "\": {";
        
        ss << "\"predecessors\": [";
        bool firstP = true;
        for (ir::ID p : b.predecessors) {
            if (!firstP) ss << ", ";
            firstP = false;
            ss << p;
        }
        ss << "], ";
        
        ss << "\"successors\": [";
        bool firstS = true;
        for (ir::ID s : b.successors) {
            if (!firstS) ss << ", ";
            firstS = false;
            ss << s;
        }
        ss << "], ";
        
        ss << "\"statements\": [";
        bool firstStmt = true;
        for (const auto& stmt : b.statements) {
            if (!firstStmt) ss << ", ";
            firstStmt = false;
            ss << "{\"" << "text\": \"" << escapeJSON(stmtToString(stmt.get())) << "\"}";
        }
        ss << "]";
        
        ss << "}";
    }
    ss << "}";
    return ss.str();
}

std::string livenessToJSON(const ir::Function& func, const liveness::LivenessResult& res) {
    std::stringstream ss;
    ss << "{";
    bool firstStmt = true;
    for (const auto& kv : func.blocks) {
        for (const auto& stmt : kv.second.statements) {
            if (!firstStmt) ss << ", ";
            firstStmt = false;
            ss << "\"" << escapeJSON(stmtToString(stmt.get())) << "\": {";
            
            ss << "\"before\": [";
            bool firstIn = true;
            if (res.liveBefore.find(stmt.get()) != res.liveBefore.end()) {
                for (const auto& var : res.liveBefore.at(stmt.get())) {
                    if (!firstIn) ss << ", ";
                    firstIn = false;
                    ss << "\"" << escapeJSON(var.name) << "\"";
                }
            }
            ss << "], ";
            
            ss << "\"after\": [";
            bool firstOut = true;
            if (res.liveAfter.find(stmt.get()) != res.liveAfter.end()) {
                for (const auto& var : res.liveAfter.at(stmt.get())) {
                    if (!firstOut) ss << ", ";
                    firstOut = false;
                    ss << "\"" << escapeJSON(var.name) << "\"";
                }
            }
            ss << "], ";
            
            ss << "\"use\": [";
            bool firstUse = true;
            auto addUse = [&](const std::string& v) {
                if (!firstUse) ss << ", ";
                firstUse = false;
                ss << "\"" << escapeJSON(v) << "\"";
            };
            switch(stmt->getType()) {
                case ir::StmtType::Assign: addUse(static_cast<const ir::AssignStmt*>(stmt.get())->src.name); break;
                case ir::StmtType::Load: addUse(static_cast<const ir::LoadStmt*>(stmt.get())->base.name); break;
                case ir::StmtType::Store: {
                    auto st = static_cast<const ir::StoreStmt*>(stmt.get());
                    addUse(st->base.name);
                    addUse(st->value.name);
                    break;
                }
                case ir::StmtType::Return: {
                    auto rt = static_cast<const ir::ReturnStmt*>(stmt.get());
                    if (rt->hasValue) addUse(rt->value.name);
                    break;
                }
                default: break;
            }
            ss << "], ";
            
            ss << "\"def\": [";
            bool firstDef = true;
            auto addDef = [&](const std::string& v) {
                if (!firstDef) ss << ", ";
                firstDef = false;
                ss << "\"" << escapeJSON(v) << "\"";
            };
            switch(stmt->getType()) {
                case ir::StmtType::Assign: addDef(static_cast<const ir::AssignStmt*>(stmt.get())->dest.name); break;
                case ir::StmtType::Load: addDef(static_cast<const ir::LoadStmt*>(stmt.get())->dest.name); break;
                case ir::StmtType::Alloc: addDef(static_cast<const ir::AllocStmt*>(stmt.get())->dest.name); break;
                default: break;
            }
            ss << "]";
            
            ss << "}";
        }
    }
    ss << "}";
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <source_file> <config_type>\n";
        return 1;
    }
    
    std::string sourceFile = argv[1];
    std::string configType = argv[2];

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
    
    auto program = frontend::ClangFrontend::translate(ast->getASTContext());
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

    liveness::LivenessSolver ls;
    liveness::LivenessResult livenessRes;
    livenessRes = ls.run(*funcToTest);
    
    solver::HRASolver solver;
    std::vector<std::string> traceEvents;
    solver::HRASolverStats stats;
    size_t mdeHits = 0, mdeMisses = 0;
    
    if (configType == "A") {
        TracingConfig<solver::BaselineHRAConfig> configA;
        solver.run(*funcToTest, &configA);
        traceEvents = configA.events;
        stats = configA.getStats();
    } else if (configType == "B") {
        TracingConfig<solver::LivenessGuidedHRAConfig> configB(&livenessRes);
        solver.run(*funcToTest, &configB);
        traceEvents = configB.events;
        stats = configB.getStats();
    } else if (configType == "C") {
        TracingConfig<mde_adapter::MDEGuidedHRAConfig> configC(&livenessRes);
        solver.run(*funcToTest, &configC);
        traceEvents = configC.events;
        stats = configC.getStats();
        mdeHits = configC.mdeHits;
        mdeMisses = configC.mdeMisses;
    } else {
        std::cerr << "Invalid config type\n";
        return 1;
    }
    
    std::cout << "{";
    std::cout << "\"config\": \"" << configType << "\", ";
    std::cout << "\"cfg\": " << cfgToJSON(*funcToTest) << ", ";
    std::cout << "\"liveness\": " << livenessToJSON(*funcToTest, livenessRes) << ", ";
    
    std::cout << "\"stats\": {";
    std::cout << "\"transfers\": " << stats.transfersExecuted << ", ";
    std::cout << "\"statementsExamined\": " << stats.statementsExamined << ", ";
    std::cout << "\"statementsPruned\": " << stats.statementsPruned << ", ";
    std::cout << "\"bindingsRemoved\": " << stats.bindingsRemoved << ", ";
    std::cout << "\"worklistIterations\": " << stats.worklistIterations << ", ";
    std::cout << "\"statesConstructed\": " << stats.statesConstructed << ", ";
    std::cout << "\"canonicalCalls\": " << stats.canonicalizeCalls << ", ";
    std::cout << "\"canonicalNew\": " << stats.newCanonicalStates << ", ";
    std::cout << "\"canonicalReused\": " << stats.reusedCanonicalStates << ", ";
    std::cout << "\"mdeHits\": " << mdeHits << ", ";
    std::cout << "\"mdeMisses\": " << mdeMisses;
    std::cout << "}, ";
    
    std::cout << "\"trace\": [";
    bool firstEv = true;
    for (const auto& ev : traceEvents) {
        if (!firstEv) std::cout << ", ";
        firstEv = false;
        std::cout << ev;
    }
    std::cout << "]";
    std::cout << "}\n";
    
    return 0;
}
