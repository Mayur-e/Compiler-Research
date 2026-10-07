#include <iostream>
#include <cassert>
#include "mdehra/mde/HRAEncoder.hpp"

using namespace mdehra;
using namespace mdehra::hra;
using namespace mdehra::mde_adapter;

ir::Variable makeVar(ir::ID id, const std::string& name) {
    ir::Variable v;
    v.id = id;
    v.name = name;
    v.isPointer = true;
    return v;
}

ir::Field makeField(ir::ID id, const std::string& name) {
    ir::Field f;
    f.id = id;
    f.name = name;
    return f;
}

void testA_EmptyState(HRAEncoder& enc) {
    HRAState s1;
    HRAState s2;
    
    StateIndex idx1 = enc.encode(s1);
    StateIndex idx2 = enc.encode(s2);
    
    assert(idx1 == idx2);
    std::cout << "Test A Passed\n";
}

void testB_OneAllocation(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {100};
    s1.graph.addNode(100);
    
    HRAState s2;
    s2.bindings[makeVar(1, "p")] = {100};
    s2.graph.addNode(100);
    
    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test B Passed\n";
}

void testC_TwoIndependentAllocations(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {100};
    s1.graph.addNode(100);

    HRAState s2;
    s2.bindings[makeVar(1, "p")] = {100};
    s2.bindings[makeVar(2, "q")] = {200};
    s2.graph.addNode(100);
    s2.graph.addNode(200);

    assert(enc.encode(s1) != enc.encode(s2));
    std::cout << "Test C Passed\n";
}

void testD_Aliasing(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {100};
    s1.bindings[makeVar(2, "q")] = {100};
    s1.graph.addNode(100);
    
    HRAState s2;
    s2.bindings[makeVar(1, "p")] = {100};
    s2.bindings[makeVar(2, "q")] = {100};
    s2.graph.addNode(100);
    
    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test D Passed\n";
}

void testE_OneFieldEdge(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 200);

    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test E Passed\n";
}

void testF_LinkedListChain(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);
    s1.graph.addEdge(200, makeField(1, "next"), 300);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 200);
    s2.graph.addEdge(200, makeField(1, "next"), 300);

    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test F Passed\n";
}

void testG_BranchProducedState(HRAEncoder& enc) {
    HRAState left;
    left.bindings[makeVar(1, "p")] = {100};
    left.graph.addNode(100);

    HRAState right;
    right.bindings[makeVar(1, "p")] = {200};
    right.graph.addNode(200);

    HRAState merged = left;
    merged.merge(right); // p -> {100, 200}, graph nodes {100, 200}
    
    HRAState direct;
    direct.bindings[makeVar(1, "p")] = {100, 200};
    direct.graph.addNode(100);
    direct.graph.addNode(200);
    
    assert(enc.encode(merged) == enc.encode(direct));
    std::cout << "Test G Passed\n";
}

void testH_SharedHeapTarget(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 300);
    s1.graph.addEdge(200, makeField(1, "next"), 300);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 300);
    s2.graph.addEdge(200, makeField(1, "next"), 300);

    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test H Passed\n";
}

void testI_SelfCycle(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 100);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 100);

    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test I Passed\n";
}

void testJ_TwoNodeCycle(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);
    s1.graph.addEdge(200, makeField(1, "next"), 100);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 200);
    s2.graph.addEdge(200, makeField(1, "next"), 100);

    assert(enc.encode(s1) == enc.encode(s2));
    std::cout << "Test J Passed\n";
}

void testL_DifferingFieldLabel(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);

    HRAState s2;
    s2.graph.addEdge(100, makeField(2, "prev"), 200); // Same nodes, different field

    assert(enc.encode(s1) != enc.encode(s2));
    std::cout << "Test L Passed\n";
}

void testM_DifferingEdgeTarget(HRAEncoder& enc) {
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 300); // Same field, diff target

    assert(enc.encode(s1) != enc.encode(s2));
    std::cout << "Test M Passed\n";
}

void testN_DifferentVariableRootBindings(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {100};
    s1.graph.addNode(100);

    HRAState s2;
    s2.bindings[makeVar(2, "q")] = {100};
    s2.graph.addNode(100);

    assert(enc.encode(s1) != enc.encode(s2));
    std::cout << "Test N Passed\n";
}

void test_SubGraphReuse(HRAEncoder& enc) {
    // Tests that MDE structurally reuses Graph parts if bindings differ
    HRAState s1;
    s1.graph.addEdge(100, makeField(1, "next"), 200);
    s1.graph.addEdge(200, makeField(1, "next"), 300);
    s1.bindings[makeVar(1, "p")] = {100};

    HRAState s2;
    s2.graph.addEdge(100, makeField(1, "next"), 200);
    s2.graph.addEdge(200, makeField(1, "next"), 300);
    s2.bindings[makeVar(2, "q")] = {100}; // Only binding differs

    StateIndex idx1 = enc.encode(s1);
    StateIndex idx2 = enc.encode(s2);

    assert(idx1 != idx2); // The states are different
    
    // We expect the GraphMDE component to have high reuse. 
    // We can't trivially assert it programmatically without peeking into MDE internals,
    // but the performance dump will show hits.
    std::cout << "Test SubGraph Reuse Passed\n";
}

void testO_IsomorphicSimple(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {1};
    s1.graph.addEdge(1, makeField(1, "next"), 2);
    
    HRAState s2;
    s2.bindings[makeVar(1, "p")] = {7};
    s2.graph.addEdge(7, makeField(1, "next"), 8);
    
    StateIndex idx1 = enc.encode(s1);
    StateIndex idx2 = enc.encode(s2);
    
    // We want to observe whether they are equal or different
    if (idx1 == idx2) {
        std::cout << "Test O: Isomorphic Simple -> EQUAL\n";
    } else {
        std::cout << "Test O: Isomorphic Simple -> DIFFERENT\n";
    }
}

void testP_IsomorphicChain(HRAEncoder& enc) {
    HRAState s1;
    s1.bindings[makeVar(1, "p")] = {1};
    s1.graph.addEdge(1, makeField(1, "next"), 2);
    s1.graph.addEdge(2, makeField(1, "next"), 3);
    
    HRAState s2;
    s2.bindings[makeVar(1, "p")] = {10};
    s2.graph.addEdge(10, makeField(1, "next"), 11);
    s2.graph.addEdge(11, makeField(1, "next"), 12);
    
    StateIndex idx1 = enc.encode(s1);
    StateIndex idx2 = enc.encode(s2);
    
    if (idx1 == idx2) {
        std::cout << "Test P: Isomorphic Chain -> EQUAL\n";
    } else {
        std::cout << "Test P: Isomorphic Chain -> DIFFERENT\n";
    }
}

int main() {
    HRAEncoder enc;
    
    testA_EmptyState(enc);
    testB_OneAllocation(enc);
    testC_TwoIndependentAllocations(enc);
    testD_Aliasing(enc);
    testE_OneFieldEdge(enc);
    testF_LinkedListChain(enc);
    testG_BranchProducedState(enc);
    testH_SharedHeapTarget(enc);
    testI_SelfCycle(enc);
    testJ_TwoNodeCycle(enc);
    testL_DifferingFieldLabel(enc);
    testM_DifferingEdgeTarget(enc);
    testN_DifferentVariableRootBindings(enc);
    
    test_SubGraphReuse(enc);
    testO_IsomorphicSimple(enc);
    testP_IsomorphicChain(enc);
    
    std::cout << enc.dump_perf();
    
    std::cout << "ALL PHASE 7 ADAPTER TESTS PASSED\n";
    return 0;
}
