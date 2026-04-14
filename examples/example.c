#include "../src/mini_alloc.h"
#include <stdio.h>

int main(void) {
    mm_set_debug(1);

    // Allocate an array of 10 integers
    int *arr = (int *)mm_malloc(10 * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }
    mm_debug_print_heap();

    for(int i = 0; i < 10; i++) {
        arr[i] = i+1;
    }
    for (int i = 0; i < 10;i++){
        printf("%d ", arr[i]);
    }
    printf("\n");


    mm_free(arr);
    mm_debug_print_heap();
    mm_free(arr); // Should log a warning but not crash
    mm_debug_print_heap();
    return 0;
}
