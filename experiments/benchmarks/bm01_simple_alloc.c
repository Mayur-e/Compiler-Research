/*
 * BM01 — Simple Allocation and Pointer Use
 * Category:  Simple heap/pointer benchmark
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * A minimal benchmark that allocates a single heap node,
 * stores a pointer value into a field, and returns the root.
 * Exercises: alloc, field store, field load, return.
 * Dead pointer: none.
 */
void* bm01_simple_alloc() {
    struct Node* p;
    struct Node* q;
    p = (struct Node*)__builtin_malloc(8);
    q = (struct Node*)__builtin_malloc(8);
    p->next = q;
    return p;
}
