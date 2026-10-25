#include <stdlib.h>
struct Cell { int *p; };
int main(void) {
    struct Cell *a = malloc(sizeof *a), *b = malloc(sizeof *b);
    int *x = malloc(sizeof *x), *y = malloc(sizeof *y);
    if (!a || !b || !x || !y) { free(a); free(b); free(x); free(y); return 2; }
    a->p = x; b->p = y;
    *x = 17; *y = 29;
    int out = *a->p + *b->p;
    free(a); free(b); free(x); free(y);
    return out == 46 ? 0 : 1;
}
