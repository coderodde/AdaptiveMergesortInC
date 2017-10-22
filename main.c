//
//  main.c
//  AdaptiveMergesortInC
//
//  Created by Rodion Efremov on 20/10/2017.
//  Copyright © 2017 coderodde.net. All rights reserved.
//

#include "net/coderodde/util/AdaptiveMergesort.h"
#include <stdio.h>

int my_cmp(const void* a, const void* b)
{
    int aa = *((int*)(a));
    int bb = *((int*)(b));
    int cmp = aa - bb;
    
    if (cmp)
    {
        return cmp;
    }
    
    return a < b ? -1 : (a > b ? 1 : 0);
}

int main(int argc, const char * argv[]) {
    int* a1 = malloc(sizeof(int)); *a1 = 1;
    int* a2 = malloc(sizeof(int)); *a2 = 2;
    int* a3 = malloc(sizeof(int)); *a3 = 3;
    int* a4 = malloc(sizeof(int)); *a4 = 4;
    int* a5 = malloc(sizeof(int)); *a5 = 5;
    
    int* arr[] = { a2, a3, a1, a2, a5, a4 };
    
    adaptive_mergesort(arr, 6, sizeof(int), my_cmp);
    return 0;
}
