#include <stdlib.h>
struct Node { struct Node *next; unsigned value; };
int main(void) {
    struct Node *head = 0;
    for (unsigned i = 0; i < 64; ++i) {
        struct Node *n = (struct Node *)malloc(sizeof *n);
        if (!n) return 2;
        n->value = i; n->next = head; head = n;
    }
    unsigned long sum = 0;
    for (struct Node *p = head; p; p = p->next) sum += p->value;
    while (head) { struct Node *q = head->next; free(head); head = q; }
    return sum == ((unsigned long)64 * (64 - 1) / 2) ? 0 : 1;
}
