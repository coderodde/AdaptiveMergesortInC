#include "net/coderodde/util/AdaptiveMergesort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

/*****************************************************************************
* This interval encodes an ascending contiguous sequence in the input array. *
* The array from, from + 1, ..., to - 2, to - 1 is an ascending sorted       *
* contiquous sequence.                                                       *
*****************************************************************************/
typedef struct interval_t {
    void* begin;
    void* end;
    struct interval_t* prev;
    struct interval_t* next;
} interval_t;

/*************************************************************************
* This run encodes an ascending contiquous sequence in the target array. *
*************************************************************************/
typedef struct run_t {
    interval_t* first_interval;
    interval_t* last_interval;
} run_t;

/********************************************
* Allocates and initializes a new interval. *
********************************************/
static interval_t* interval_t_alloc(void* begin, void* end)
{
    interval_t* result = malloc(sizeof *result);
    
    if (!result)
    {
        abort();
    }
    
    result->begin = begin;
    result->end = end;
    return result;
}

/*****************************************************************************
* Allocates and initializes a new run. It will consist of a single interval. *
*****************************************************************************/
static run_t* run_t_alloc(void* begin, void* end)
{
    interval_t* interval = interval_t_alloc(begin, end);
    run_t* run = malloc(sizeof *run);
    
    if (!run)
    {
        abort();
    }
    
    run->first_interval = interval;
    run->last_interval  = interval;
    interval->prev = NULL;
    interval->next = NULL;
    return run;
}

/************************************************************
* A simple data structure for managing runs during sorting. *
************************************************************/
typedef struct run_queue_t {
    run_t** run_array;
    size_t head;
    size_t tail;
    size_t mask;
    size_t size;
} run_queue_t;

/******************************************************************************
* Returns an integer whose only set bit is the highest order bit of 'number'. *
******************************************************************************/
static size_t highest_one_bit(size_t number)
{
    size_t result = 1;
    
    while (number >>= 1)
    {
        result <<= 1;
    }
    
    return result;
}

/******************************************************************
* Returns a smallest power of two that is not less than 'number'. *
******************************************************************/
static size_t ceil_to_power_of_two(size_t number)
{
    size_t result = highest_one_bit(number);
    return result != number ? result << 1 : result;
}

/***********************************
* Allocates a new empty run queue. *
***********************************/
static run_queue_t* run_queue_t_alloc(size_t capacity)
{
    run_queue_t* run_queue = malloc(sizeof *run_queue);
    
    if (!run_queue)
    {
        abort();
    }
    
    capacity = ceil_to_power_of_two(capacity);
    run_queue->mask = capacity - 1;
    run_queue->head = 0;
    run_queue->tail = 0;
    run_queue->size = 0;
    run_queue->run_array = malloc((sizeof *run_queue->run_array) * capacity);
    return run_queue;
}

/************************************************
* Deallocates the memory used by the run queue. *
************************************************/
static void run_queue_t_free(run_queue_t* run_queue)
{
    // TODO: Possibly free the only run.
    free(run_queue->run_array);
    free(run_queue);
}

/**********************************************
* Appends 'run' to the tail of the run queue. *
**********************************************/
static void run_queue_t_enqueue(run_queue_t* run_queue, run_t* run)
{
    run_queue->run_array[run_queue->tail] = run;
    run_queue->tail = (run_queue->tail + 1) & run_queue->mask;
    run_queue->size++;
}

/******************************************************************
* Extends the last run in the run queue by 'run_length' elements. *
******************************************************************/
static void run_queue_t_add_to_last_run(run_queue_t* run_queue,
                                        size_t run_length)
{
    size_t index = (run_queue->tail - 1) & run_queue->mask;
    run_queue->run_array[index]->first_interval->end += run_length;
}

/**************************************
* Removes the first run in the queue. *
**************************************/
static run_t* run_queue_t_dequeue(run_queue_t* run_queue)
{
    run_t* run = run_queue->run_array[run_queue->head];
    run_queue->head = (run_queue->head + 1) & run_queue->mask;
    run_queue->size--;
    return run;
}

/*********************************************************
* Returns the number of runs currently in the run queue. *
*********************************************************/
static size_t run_queue_t_size(run_queue_t* run_queue)
{
    return run_queue->size;
}

/**********************************************************************
* This run queue builder is responsible for constructing a run queue. *
**********************************************************************/
typedef struct run_queue_builder_t {
    run_queue_t* run_queue;
    void* base;
    size_t element_size;
    size_t element_count;
    void* head;
    void* left;
    void* right;
    void* last;
    void* aux;
    int previous_run_was_descending;
    int (*cmp)(const void*, const void*);
} run_queue_builder_t;

/*************************************
* Initializes the run queue builder. *
*************************************/
static run_queue_builder_t*
run_queue_builder_t_alloc(void* base,
                          size_t element_count,
                          size_t element_size,
                          int (*cmp)(const void*, const void*))
{
    run_queue_builder_t* run_queue_builder = malloc(sizeof *run_queue_builder);
    
    if (!run_queue_builder)
    {
        abort();
    }
    
    run_queue_builder->aux = malloc(element_size);
    
    if (!run_queue_builder->aux)
    {
        abort();
    }
    
    run_queue_builder->run_queue = run_queue_t_alloc(element_count);
    run_queue_builder->base = base;
    run_queue_builder->element_count = element_count;
    run_queue_builder->element_size = element_size;
    run_queue_builder->left = base;
    run_queue_builder->right = base + element_size;
    run_queue_builder->last = base + (element_count - 1) * element_size;
    run_queue_builder->previous_run_was_descending = 0;
    run_queue_builder->cmp = cmp;
    return run_queue_builder;
}

/*********************************************
* Scans an ascending run in the input array. *
*********************************************/
static void run_queue_builder_t_scan_ascending_run(
            run_queue_builder_t* run_queue_builder)
{
    void* left  = run_queue_builder->left;
    void* right = run_queue_builder->right;
    void* last  = run_queue_builder->last;
    void* head  = run_queue_builder->head;
    
    int (*cmp)(const void*, const void*) = run_queue_builder->cmp;
    run_queue_t* run_queue = run_queue_builder->run_queue;
    
    size_t element_size = run_queue_builder->element_size;
    run_t* run;
    
    while (left < last && cmp(left, right) <= 0)
    {
        left = right;
        right += element_size;
    }
    
    if (run_queue_builder->previous_run_was_descending)
    {
        if (cmp(head - element_size, head) <= 0)
        {
            run_queue_t_add_to_last_run(run_queue, right - head);
        }
        else
        {
            run = run_t_alloc(head, right);
            run_queue_t_enqueue(run_queue, run);
        }
    }
    else
    {
        run = run_t_alloc(head, right);
        run_queue_t_enqueue(run_queue, run);
    }
    
    run_queue_builder->previous_run_was_descending = 0;
    run_queue_builder->left = left;
    run_queue_builder->right = right;
    run_queue_builder->head = head;
}

/**************************************************************************
* Reverses a strictly descending run into an ascending one. Strictness is *
* required in order to keep the entire sorting algorithm stable.          *
**************************************************************************/
static void run_queue_builder_t_reverse_run(
                                        run_queue_builder_t* run_queue_builder,
                                        run_t* run)
{
    size_t element_size = run_queue_builder->element_size;
    void* end = run->first_interval->end - element_size;
    void* begin = run->first_interval->begin;
    void* aux = run_queue_builder->aux;
    
    while (begin < end)
    {
        memcpy(aux, begin, element_size);
        memcpy(begin, end, element_size);
        memcpy(end, aux, element_size);
        
        begin += element_size;
        end -= element_size;
    }
}

/*********************************************
* Scans a descending run in the input array. *
*********************************************/
static void run_queue_builder_t_scan_descending_run(
            run_queue_builder_t* run_queue_builder)
{
    void* left  = run_queue_builder->left;
    void* right = run_queue_builder->right;
    void* last  = run_queue_builder->last;
    void* head  = run_queue_builder->head;
    
    int (*cmp)(const void*, const void*) = run_queue_builder->cmp;
    run_queue_t* run_queue = run_queue_builder->run_queue;
    
    size_t element_size = run_queue_builder->element_size;
    run_t* run;
    
    while (left != last && cmp(left, right) > 0)
    {
        left = right;
        right += element_size;
    }
    
    if (run_queue_builder->previous_run_was_descending)
    {
        if (cmp(head - element_size, head) <= 0)
        {
            run_queue_t_add_to_last_run(run_queue, right - head);
        }
        else
        {
            run = run_t_alloc(head, right);
            run_queue_builder_t_reverse_run(run_queue_builder, run);
            run_queue_t_enqueue(run_queue, run);
        }
    }
    else
    {
        run = run_t_alloc(head, right);
        run_queue_builder_t_reverse_run(run_queue_builder, run);
        run_queue_t_enqueue(run_queue, run);
    }
    
    run_queue_builder->previous_run_was_descending = 1;
    run_queue_builder->left = left;
    run_queue_builder->right = right;
    run_queue_builder->head = head;
}

void super(run_queue_t* run_queue, size_t element_size)
{
    run_t* run;
    interval_t* interval;
    void* shit;
    
    while (run_queue_t_size(run_queue) > 0)
    {
        run = run_queue_t_dequeue(run_queue);
        interval = run->first_interval;
        
        for (shit = interval->begin; shit < interval->end; shit += element_size)
        {
            printf("%d ", **((int**)(shit)));
        }
        
        puts("");
    }
}

/***********************************************************
* This function is responsible for building the run queue. *
***********************************************************/
static run_queue_t* run_queue_builder_t_run(
                    run_queue_builder_t* run_queue_builder)
{
    int (*cmp)(const void*, const void*) = run_queue_builder->cmp;
    size_t element_size = run_queue_builder->element_size;
    void* last = run_queue_builder->last;
    
    while (run_queue_builder->left < run_queue_builder->last)
    {
        run_queue_builder->head = run_queue_builder->left;
        
        if (cmp(run_queue_builder->left, run_queue_builder->right) <= 0) {
            run_queue_builder->left = run_queue_builder->right;
            run_queue_builder->right += element_size;
            run_queue_builder_t_scan_ascending_run(run_queue_builder);
        }
        else
        {
            run_queue_builder->left = run_queue_builder->right;
            run_queue_builder->right += element_size;
            run_queue_builder_t_scan_descending_run(run_queue_builder);
        }
        
        run_queue_builder->left = run_queue_builder->right;
        run_queue_builder->right += element_size;
    }
    
    if (run_queue_builder->left == run_queue_builder->last)
    {
        /***************************************************************
        * Deal with a single element run at the very tail of the input *
        * array range.                                                 *
        ***************************************************************/
        if (cmp(last - element_size, run_queue_builder->last) <= 0)
        {
            run_queue_t_add_to_last_run(run_queue_builder->run_queue,
                                        element_size);
        }
        else
        {
            run_queue_t_enqueue(
                        run_queue_builder->run_queue,
                        run_t_alloc(run_queue_builder->left,
                                    run_queue_builder->left + element_size));
        }
    }
    
    super(run_queue_builder->run_queue, run_queue_builder->element_size);
    return run_queue_builder->run_queue;
}

/*******************************************************************************
* Returns the pointer to the first element in the range which compares greater *
* than 'value'.                                                                *
*******************************************************************************/
static void* upper_bound(void* base,
                         size_t num,
                         size_t size,
                         void* value,
                         int (*cmp)(const void*, const void*))
{
    size_t count = num;
    size_t step;
    void* it;
    
    while (count)
    {
        it = base;
        step = count >> 1;
        it += step * size;
        
        if (cmp(it, value) <= 0)
        {
            base = it + size;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }
    
    return base;
    
}
                        
/*******************************************************************************
* Returns the pointer to the first element in the range which does not compare *
* less than 'value'.                                                           *
*******************************************************************************/
static void* lower_bound(void* base,
                         size_t num,
                         size_t size,
                         void* value,
                         int (*cmp)(const void*, const void*))
{
    size_t count = num;
    size_t step;
    void* it;
    
    while (count)
    {
        it = base;
        step = count >> 1;
        it += step * size;
        
        if (cmp(it, value) < 0)
        {
            base = it + size;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }
    
    return base;
}

static void* find_upper_bound(void* base,
                              size_t num,
                              size_t size,
                              void* value,
                              int (*cmp)(const void*, const void*))
{
    size_t bound = 1;
    
    while (bound < num && cmp(base + bound * size, value) < 0) {
        bound <<= 1;
    }
    
    return upper_bound(base + (bound >> 1) * size,
                       MIN(bound, num),
                       size,
                       value,
                       cmp);
}

static void* find_lower_bound(void* base,
                              size_t num,
                              size_t size,
                              void* value,
                              int (*cmp)(const void*, const void*))
{
    size_t bound = 1;
    
    while (bound < num && cmp(base + bound * size, value) < 0) {
        bound <<= 1;
    }
    
    return lower_bound(base + (bound >> 1) * size,
                       MIN(bound, num),
                       size,
                       value,
                       cmp);
}

void adaptive_mergesort(void* base,
                        size_t num,
                        size_t size,
                        int (*compar)(const void*, const void*))
{
    void* aux = malloc(num * size);
    memcpy(aux, base, num * size);
    size_t runs_left;
    run_queue_builder_t* run_queue_builder = run_queue_builder_t_alloc(base,
                                                                       num,
                                                                       size,
                                                                       compar);
    run_queue_t* run_queue = run_queue_builder_t_run(run_queue_builder);
    runs_left = run_queue_t_size(run_queue);
    
    while (run_queue_t_size(run_queue) > 1)
    {
        switch (runs_left)
        {
            case 1:
                run_queue_t_enqueue(run_queue, run_queue_t_dequeue(run_queue));
                /****************
                * FAll through! *
                ****************/
                
            case 0:
                runs_left = run_queue_t_size(run_queue);
                continue;
        }
        
        
    }
}
