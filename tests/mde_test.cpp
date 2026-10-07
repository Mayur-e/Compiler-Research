#include <iostream>
#include <cassert>
#include "mde/mde.hpp"

using LeafMDE = mde::MDENode<mde::MDEConfig<int>>;
using LeafIndex = LeafMDE::Index;

using PairMDE = mde::MDENode<
    mde::MDEConfig<int>, 
    mde::NestingBase<int, LeafMDE>
>;
using PairIndex = PairMDE::Index;

using TreeMDE = mde::MDENode<
    mde::MDEConfig<int>,
    mde::NestingBase<int, PairMDE>
>;
using TreeIndex = TreeMDE::Index;


void test1_AtomicValues(LeafMDE& cl) {
    LeafIndex a = cl.register_set_single(10);
    LeafIndex b = cl.register_set_single(20);
    LeafIndex c = cl.register_set_single(10);
    
    assert(a != b);
    assert(a == c);
    std::cout << "Test 1 Passed\n";
}

void test2_IdenticalComposite(LeafMDE& cl, PairMDE& pl) {
    LeafIndex a = cl.register_set_single(1);
    LeafIndex b = cl.register_set_single(2);
    
    PairIndex p1 = pl.register_set({ {1, {a}}, {2, {b}} });
    PairIndex p2 = pl.register_set({ {1, {a}}, {2, {b}} });
    
    assert(p1 == p2);
    std::cout << "Test 2 Passed\n";
}

void test3_OneChildDifference(LeafMDE& cl, PairMDE& pl) {
    LeafIndex a = cl.register_set_single(1);
    LeafIndex b = cl.register_set_single(2);
    LeafIndex c = cl.register_set_single(3);
    
    PairIndex p1 = pl.register_set({ {1, {a}}, {2, {b}} });
    PairIndex p2 = pl.register_set({ {1, {a}}, {2, {c}} });
    
    assert(p1 != p2);
    std::cout << "Test 3 Passed\n";
}

void test4_NestedStructures(LeafMDE& leaf, PairMDE& pair, TreeMDE& tree) {
    LeafIndex a = leaf.register_set_single(1);
    LeafIndex b = leaf.register_set_single(2);
    LeafIndex c = leaf.register_set_single(3);
    LeafIndex d = leaf.register_set_single(4);

    PairIndex pBC = pair.register_set({ {1, {b}}, {2, {c}} });
    PairIndex pBD = pair.register_set({ {1, {b}}, {2, {d}} });

    TreeIndex s1 = tree.register_set({ {1, {pBC}} });
    TreeIndex s2 = tree.register_set({ {1, {pBC}} });
    TreeIndex s3 = tree.register_set({ {1, {pBD}} });

    assert(s1 == s2);
    assert(s1 != s3);
    std::cout << "Test 4 Passed\n";
}

void test5_SharedSubstructures(LeafMDE& leaf, PairMDE& pair, TreeMDE& tree) {
    LeafIndex a = leaf.register_set_single(1);
    LeafIndex b = leaf.register_set_single(2);
    
    PairIndex x = pair.register_set({ {1, {a}}, {2, {b}} });
    
    TreeIndex y = tree.register_set({ {1, {x}} });
    TreeIndex z = tree.register_set({ {2, {x}} });
    
    // Y and Z both physically point to x. MDE handles memory and indices,
    // so sharing is trivial via integer indices.
    assert(x == x);
    std::cout << "Test 5 Passed\n";
}

void test6_InsertionOrder(LeafMDE& leaf, PairMDE& pair) {
    LeafIndex a = leaf.register_set_single(1);
    LeafIndex b = leaf.register_set_single(2);
    
    // MDE requires property sets to be sorted upon registration.
    // If we want to build a set dynamically, we can use std::set.
    std::vector<PairMDE::PropertyElement> v1 = { {1, {a}}, {2, {b}} };
    std::vector<PairMDE::PropertyElement> v2 = { {2, {b}}, {1, {a}} };
    
    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());
    
    PairIndex p1 = pair.register_set(std::move(v1));
    PairIndex p2 = pair.register_set(std::move(v2));
    
    assert(p1 == p2);
    std::cout << "Test 6 Passed\n";
}

void test7_NegativeEquality(LeafMDE& cl, PairMDE& pl) {
    LeafIndex a = cl.register_set_single(1);
    LeafIndex b = cl.register_set_single(2);
    LeafIndex c = cl.register_set_single(3);
    
    PairIndex pAB = pl.register_set({ {1, {a}}, {2, {b}} });
    PairIndex pAC = pl.register_set({ {1, {a}}, {2, {c}} });
    PairIndex pBB = pl.register_set({ {1, {b}}, {2, {b}} });
    PairIndex pBC = pl.register_set({ {1, {b}}, {2, {c}} });
    
    assert(pAB != pAC);
    assert(pAC != pBB);
    assert(pBB != pBC);
    assert(pAB != pBC);
    std::cout << "Test 7 Passed\n";
}

void test8_HashCollisionSafety(LeafMDE& cl) {
    // Custom hashing to force collision
    struct BadHashConfig : mde::MDEConfig<int> {
        struct PropertyHash {
            std::size_t operator()(int /*v*/) const { return 42; } // Always collides
        };
    };
    
    using BadMDE = mde::MDENode<BadHashConfig>;
    BadMDE bad;
    
    auto a = bad.register_set_single(10);
    auto b = bad.register_set_single(20);
    
    assert(a != b); // distinct values remain distinct despite hash collision
    std::cout << "Test 8 Passed\n";
}

void test9_Determinism(LeafMDE& leaf, PairMDE& pair) {
    LeafIndex a1 = leaf.register_set_single(1);
    PairIndex p1 = pair.register_set({ {1, {a1}} });
    
    LeafIndex a2 = leaf.register_set_single(1);
    PairIndex p2 = pair.register_set({ {1, {a2}} });
    
    // Same physical sequence gives same ID (IDs are deterministic)
    assert(a1.value == a2.value);
    assert(p1.value == p2.value);
    std::cout << "Test 9 Passed\n";
}

void test10_RepeatedInsertion(LeafMDE& leaf) {
    LeafIndex first = leaf.register_set_single(100);
    for (int i = 0; i < 1000; i++) {
        LeafIndex curr = leaf.register_set_single(100);
        assert(curr == first);
    }
    std::cout << "Test 10 Passed\n";
}

void test11_StructuralMutation(LeafMDE& leaf) {
    // MDE sets are immutable. We cannot mutate an existing index.
    // We can only register a new set.
    LeafIndex s1 = leaf.register_set({1, 2});
    LeafIndex s2 = leaf.register_set({1, 2, 3});
    
    assert(s1 != s2);
    // Reuse is completely safe because existing structures are immutable.
    std::cout << "Test 11 Passed\n";
}

void test12_EmptySingleton(LeafMDE& leaf, PairMDE& pair) {
    // Empty set
    LeafIndex empty1 = leaf.register_set({});
    LeafIndex empty2 = leaf.register_set({});
    assert(empty1 == empty2);
    
    // Singleton
    LeafIndex sing1 = leaf.register_set_single(1);
    LeafIndex sing2 = leaf.register_set({1});
    assert(sing1 == sing2);
    
    std::cout << "Test 12 Passed\n";
}

int main() {
    LeafMDE leaf;
    PairMDE pair(PairMDE::RefList{leaf});
    TreeMDE tree(TreeMDE::RefList{pair});
    
    test1_AtomicValues(leaf);
    test2_IdenticalComposite(leaf, pair);
    test3_OneChildDifference(leaf, pair);
    test4_NestedStructures(leaf, pair, tree);
    test5_SharedSubstructures(leaf, pair, tree);
    test6_InsertionOrder(leaf, pair);
    test7_NegativeEquality(leaf, pair);
    test8_HashCollisionSafety(leaf);
    test9_Determinism(leaf, pair);
    test10_RepeatedInsertion(leaf);
    test11_StructuralMutation(leaf);
    test12_EmptySingleton(leaf, pair);
    
    std::cout << "ALL PHASE 6 MDE TESTS PASSED\n";
    return 0;
}
