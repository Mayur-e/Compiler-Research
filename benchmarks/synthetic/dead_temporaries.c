#include <stdlib.h>
struct Node { struct Node *next; int value; };
int main(void) {
    struct Node *live = malloc(sizeof *live);
    if (!live) return 2;
    live->next = 0; live->value = 7;
    for (int i = 0; i < 100; ++i) {
        struct Node *dead = malloc(sizeof *dead);
        if (!dead) break;
        dead->next = 0; dead->value = i;
        free(dead);
    }
    int result = live->value;
    free(live);
    return result == 7 ? 0 : 1;
}
