/*
 * BM03 — Dead Pointer Temporaries
 * Category:  Dead-pointer-heavy program
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * Allocates several nodes that are never used after assignment
 * (dead pointers). Only the last returned pointer is live.
 * Exercises: alloc (multiple), pointer assignment, dead allocs,
 * return (one live path).
 *
 * Liveness should eliminate tracking of t1/t2 early,
 * giving B and C fewer states than A.
 *
 * Dead pointers: t1, t2 (never used after allocation)
 * Live pointer: result (returned)
 */
void* bm03_dead_pointers() {
    struct Node* t1;
    struct Node* t2;
    struct Node* result;
    t1 = (struct Node*)__builtin_malloc(8);
    t2 = (struct Node*)__builtin_malloc(8);
    result = (struct Node*)__builtin_malloc(8);
    return result;
}
