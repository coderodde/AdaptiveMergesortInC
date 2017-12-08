#include "net/coderodde/util/AdaptiveMergesort.h"
#include <stdio.h>

int my_cmp(const void* a, const void* b)
{
    int aa = **(int**)a;
    int bb = **(int**)b;
    int cmp = aa - bb;
    
    if (cmp)
    {
        return cmp;
    }
    
    return (int)(a - b);
}

int main(int argc, const char * argv[]) {
    int* a1 = malloc(sizeof(int)); *a1 = 1;
    int* a2 = malloc(sizeof(int)); *a2 = 2;
    int* a3 = malloc(sizeof(int)); *a3 = 3;
    int* a4 = malloc(sizeof(int)); *a4 = 4;
    int* a5 = malloc(sizeof(int)); *a5 = 5;
    size_t sz;
    
    int* arr[] = { a2, a1, a3, a4, a2 };
    
    adaptive_mergesort(arr, 5, sizeof(int*), my_cmp);
    
    for (sz = 0; sz < 5; ++sz)
    {
        printf("%d ", *arr[sz]);
    }
    
    
    /*int* arr2[] = { a1, a1, a2, a4, a4, a5 };
    void* yeah = lower_bound(arr2, 5, sizeof(int*), &a4, my_cmp);
    printf("%d\n", **(int**)(yeah));*/
    return 0;
}
