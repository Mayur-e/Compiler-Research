void test() {
    struct Node* p;
    struct Node* q;
    p = (struct Node*)__builtin_malloc(8);
    p->next = q;
    q = p->next;
}
