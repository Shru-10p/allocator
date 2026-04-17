#include "../src/alloc.h"
#include <stdio.h>

int main(void) {
    set_debug(1);

    // Allocate an array of 10 integers
    int *arr = (int *)my_malloc(10 * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }
    debug_print_heap();

    for(int i = 0; i < 10; i++) {
        arr[i] = i+1;
    }
    for (int i = 0; i < 10;i++){
        printf("%d ", arr[i]);
    }
    printf("\n");


    my_free(arr);
    debug_print_heap();
    return 0;
}
