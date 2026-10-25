/*
 * BM07 — Multi-Field Graph Structure
 * Category:  Larger pointer-intensive program / multiple fields
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * Constructs a small graph-like structure using both the 'next'
 * and 'prev' fields of Node. Three nodes are linked bidirectionally.
 * An additional dead temporary is included to also exercise
 * liveness pruning.
 *
 * Heap objects:  3 (a, b, c)
 * Dead pointers: 1 (tmp, allocated but never used after that)
 * Field stores:  4 (a->next=b, b->next=c, b->prev=a, c->prev=b)
 * Field loads:   1 (verify = a->next)
 * Both next and prev fields are exercised.
 *
 * This is the largest benchmark. It combines:
 *   - multiple heap objects
 *   - multi-field nodes (both next and prev)
 *   - dead pointer temporaries
 *   - field loads and stores
 */
void* bm07_multi_field_graph() {
    struct Node* a;
    struct Node* b;
    struct Node* c;
    struct Node* tmp;
    struct Node* verify;

    a = (struct Node*)__builtin_malloc(8);
    b = (struct Node*)__builtin_malloc(8);
    c = (struct Node*)__builtin_malloc(8);
    tmp = (struct Node*)__builtin_malloc(8);

    a->next = b;
    b->next = c;
    b->prev = a;
    c->prev = b;

    verify = a->next;

    return verify;
}
