#include <stdlib.h>
struct Node { struct Node *next; struct Node *prev; int value; };
int main(void) {
    struct Node *a = malloc(sizeof *a), *b = malloc(sizeof *b);
    if (!a || !b) { free(a); free(b); return 2; }
    a->next = b; a->prev = 0; b->next = 0; b->prev = a;
    struct Node *x = a->next;
    struct Node *y = b;
    int same = (x == y);
    free(a); free(b);
    return same ? 0 : 1;
}
