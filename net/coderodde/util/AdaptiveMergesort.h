#ifndef NET_CODERODDE_UTIL_ADAPTIVE_MERGESORT_H
#define NET_CODERODDE_UTIL_ADAPTIVE_MERGESORT_H

#include <stdlib.h>

void adaptive_mergesort(void* base,
                        size_t num,
                        size_t size,
                        int (*compar)(const void*, const void*));
    
#endif /* NET_CODERODDE_UTIL_ADAPTIVE_MERGESORT_H */
