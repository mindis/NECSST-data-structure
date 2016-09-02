#include <stdio.h>
#include "art.h"

int main()
{
    int len;
    char buf[512];
    FILE *f = fopen("./words.txt", "r");

    art_tree t;
    int res = art_tree_init(&t);

    uintptr_t line = 1;
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';
        printf("\ninserting %s\n", buf);

        res == art_insert(&t, (unsigned char*)buf, len, (void*)line);
        if (art_size(&t) != line) {
            printf("error line: %d\n", line);
            exit(1);
        }

        print_node((linked_node*)t.root);
        line++;
    }

    res = art_tree_destroy(&t);
}
