void test() {
    struct Node* a;
    struct Node* b;
    a = (struct Node*)__builtin_malloc(8);
    b = (struct Node*)__builtin_malloc(8);
    a->next = b;
}
