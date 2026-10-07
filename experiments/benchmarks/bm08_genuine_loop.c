/*
 * BM08 — Genuine Loop with CFG Back-Edge (while traversal)
 * Category:  Loop-heavy pointer manipulation (genuine CFG back-edge)
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * A while-loop traverses a linked list by following the next-field
 * pointer on each iteration: p = p->next.
 *
 * The Clang CFG for `while (p->next)` produces:
 *   - A loop-header block (the condition block) with TWO successors:
 *       loop body (taken when condition is true)
 *       loop exit (taken when condition is false)
 *   - A loop-body block whose SUCCESSOR is the loop-header block.
 *     This successor edge is the BACK-EDGE.
 *
 * IR profile (per iteration of the worklist):
 *   - LoadStmt in the body:  p = p->next
 *   - The HRA worklist revisits the loop header multiple times
 *     until the exit state reaches fixed point.
 *
 * This benchmark has:
 *   Heap objects:  2 (head and a second node linked via next)
 *   Field stores:  1 (head->next = second)
 *   Field loads:   1 per loop iteration (p = p->next, in the loop body)
 *   Branches:      1 (loop condition)
 *   Real back-edge: YES — loop body -> loop header
 *   Fixpoint iters: >1 (worklist must revisit loop header)
 *
 * Supported C constructs only:
 *   - malloc (alloc)
 *   - pointer assignment (assign)
 *   - field store (store)
 *   - field load (load)
 *   - while loop with pointer comparison (condition → CFG structure)
 *   - return
 *
 * NOT used: free(), pointer arithmetic, unions, function calls
 *           other than __builtin_malloc, interprocedural patterns.
 */
void* bm08_genuine_loop() {
    struct Node* head;
    struct Node* second;
    struct Node* p;
    head   = (struct Node*)__builtin_malloc(8);
    second = (struct Node*)__builtin_malloc(8);
    head->next = second;
    p = head;
    while (p->next) {
        p = p->next;
    }
    return p;
}
