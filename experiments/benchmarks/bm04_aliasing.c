/*
 * BM04 — Aliasing and Shared Targets
 * Category:  Aliasing / shared-target behavior
 * Source:    Synthetic/controlled
 * License:   N/A (project-internal)
 *
 * Two variables alias the same heap node.
 * A field store through one alias and a field load through the
 * other exercises the aliasing representation in HRA.
 *
 * Heap objects: 2 (node, target)
 * Aliases: p and alias both point to same node
 * Field stores: 1 (via p)
 * Field loads:  1 (via alias)
 *
 * This is meaningful for MDE: p and alias share the same
 * underlying HRA node, creating structural sharing opportunity.
 */
void* bm04_aliasing() {
    struct Node* p;
    struct Node* alias;
    struct Node* target;
    struct Node* loaded;
    p = (struct Node*)__builtin_malloc(8);
    target = (struct Node*)__builtin_malloc(8);
    alias = p;
    p->next = target;
    loaded = alias->next;
    return loaded;
}
