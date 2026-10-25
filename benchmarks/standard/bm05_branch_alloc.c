/*
 * BM05 — Branch-Heavy Pointer Manipulation
 * Category:  Branch-heavy pointer manipulation
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * A branch (if/else) allocates different heap nodes on each path
 * and returns a merged result. The join point sees two possible
 * HRA states.
 *
 * Branch: 1 (if cond)
 * Heap objects: up to 2 (one per path)
 * Field stores:  1 per path (next pointer)
 * Live at join:  result
 *
 * This exercises the CFG join-point state merging in HRA.
 * MDE should canonicalize equivalent structural shapes from
 * both branches if they have the same allocation-site signature.
 */
void* bm05_branch_alloc(int cond) {
    struct Node* a;
    struct Node* b;
    struct Node* result;
    if (cond) {
        a = (struct Node*)__builtin_malloc(8);
        b = (struct Node*)__builtin_malloc(8);
        a->next = b;
        result = a;
    } else {
        b = (struct Node*)__builtin_malloc(8);
        a = (struct Node*)__builtin_malloc(8);
        b->next = a;
        result = b;
    }
    return result;
}
