#include "../src/alloc.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    set_debug(0);

    char *name = (char *)my_malloc(32);
    if (!name) {
        fprintf(stderr, "my_malloc failed\n");
        return 1;
    }
    debug_print_heap();

    strcpy(name, "hello allocator!");
    printf("%s\n", name);

    int *values = (int *)my_calloc(5, sizeof(*values));
    if (!values) {
        fprintf(stderr, "my_calloc failed\n");
        my_free(name);
        return 1;
    }
    debug_print_heap();

    printf("calloc values (should start at 0): ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", values[i]);
    }
    printf("\n");

    for (int i = 0; i < 5; i++) {
        values[i] = (i * 10);
    }

    printf("updated values: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", values[i]);
    }
    printf("\n");

    my_free(values);
    my_free(name);
    debug_print_heap();

    return 0;
}
