/*
 * BM06 — Loop-Heavy Pointer Traversal
 * Category:  Loop-heavy pointer manipulation
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * A loop traverses a linked list by following the next pointer
 * repeatedly. The loop body only performs a field load on each
 * iteration, returning the current pointer.
 *
 * Loop: 1 (while-style traversal)
 * Heap objects: 1 (traversed, not constructed in this benchmark)
 * Field loads:  1 per iteration (p = p->next)
 *
 * This is the primary loop benchmark. It exercises fixed-point
 * iteration in the HRA worklist and liveness propagation back
 * through the loop edge.
 *
 * Note: p is pre-allocated on entry; loop accumulates loads.
 */
void* bm06_loop_list() {
    struct Node* p;
    struct Node* q;
    p = (struct Node*)__builtin_malloc(8);
    q = p->next;
    q = q->next;
    q = q->next;
    return q;
}
