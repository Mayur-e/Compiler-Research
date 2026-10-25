#include <stdlib.h>
struct Node { struct Node *next; int value; };
int main(void) {
    struct Node *head = 0;
    for (int i = 0; i < 64; ++i) {
        struct Node *n = malloc(sizeof *n);
        if (!n) return 2;
        n->value = i; n->next = head; head = n;
    }
    long sum = 0;
    for (struct Node *p = head; p; p = p->next) sum += p->value;
    while (head) { struct Node *next = head->next; free(head); head = next; }
    return sum == 2016 ? 0 : 1;
}
