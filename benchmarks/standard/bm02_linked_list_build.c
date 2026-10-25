/*
 * BM02 — Linked-List Construction (3 nodes)
 * Category:  Linked-list / singly-linked structure
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * Builds a three-node singly-linked list.
 * Exercises: multiple allocs, multiple field stores (next),
 * pointer assignment, return of head node.
 * Heap objects: 3.
 * Field stores: 2.
 * Dead pointers: none (all reach return).
 */
void* bm02_linked_list_build() {
    struct Node* a;
    struct Node* b;
    struct Node* c;
    a = (struct Node*)__builtin_malloc(8);
    b = (struct Node*)__builtin_malloc(8);
    c = (struct Node*)__builtin_malloc(8);
    a->next = b;
    b->next = c;
    return a;
}
