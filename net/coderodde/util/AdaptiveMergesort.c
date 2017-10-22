#include "AdaptiveMergesort.h"
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
* This interval encodes an ascending contiguous sequence in the input array. *
* The array from, from + 1, ..., to - 2, to - 1 is an ascending sorted       *
* contiquous sequence.                                                       *
*****************************************************************************/
typedef struct interval_t {
    void* begin;
    void* end;
    interval_t* prev;
    interval_t* next;
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
static interval_t_alloc(void* begin, void* end)
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
    run_queue->run_array = malloc((sizeof *run_array) * capacity);
    
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
static run_queue_builder_t* run_queue_builder_t_alloc(
                                void* base,
                                size_t element_size,
                                size_t element_count,
                                int (*cmp)(const void*, const void*))
{
    run_queue_builder_t* run_queue_builder = malloc(sizeof *run_queue_builder));
    
    if (!run_queue_builder)
    {
        abort();
    }
    
    run_queue_builder->aux = malloc(element_size);
    
    if (!run_queue->aux)
    {
        abort();
    }
    
    run_queue_builder->run_queue = run_queue_t_alloc(element_count);
    run_queue_buillder->base = base;
    run_queue_builder->element_size = element_size;
    run_queue_builder->element_count = element_count;
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
    
    while (left != last && cmp(left, right) <= 0)
    {
        left = right;
        right += element_size;
    }
    
    if (run_queue_builder->previous_run_was_descending)
    {
        if (cmp(head - element_size, head) <= 0)
        {
            run_queue_t_add_to_last_run(run_queue,
                                       (right - head) * element_size);
        }
        else
        {
            run = run_t_alloc(head, right);
            run_queue_t_enqueue(run);
        }
    }
    else
    {
        run = run_t_alloc(head, right);
        run_queue_t_enqueue(run);
    }
    
    run_queue_builder->previous_run_was_descending = false;
    run_queue_builder->left = left;
    run_queue_builder->right = right;
}

static void run_queue_builder_t_reverse_run(
                            run_queue_builder_t* run_queue_builder,
                            run_t* run)
{
    size_t element_size = run_queue_builder->element_size;
    void* end = run->end - element_size;
    void* begin = run->begin;
    void* aux = run_queue_builder->base;
    
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
    
    while (left != last && cmp(left, right) > 0)
    {
        left = right;
        right += element_size;
    }
    
    if (run_queue_builder->previous_run_was_descending)
    {
        if (cmp(head - element_size, head) <= 0)
        {
            run_queue_t_add_to_last_run(run_queue,
                                        (right - head) * element_size);
        }
        else
        {
            run = run_t_alloc(head, right);
            run_queue_builder_t_reverse_run(run, element_size);
            run_queue_t_enqueue(run);
        }
    }
    else
    {
        run = run_t_alloc(head, right);
        run_queue_builder_t_reverse_run(run, element_size);
        run_queue_t_enqueue(run);
    }
    
    run_queue_builder->previous_run_was_descending = true;
    run_queue_builder->left = left;
    run_queue_builder->right = right;
}

/***********************************************************
* This function is responsible for building the run queue. *
***********************************************************/
static run_queue* run_queue_builder_t_run(
                  run_queue_builder_t* run_queue_builder)
{
    int (*cmp)(const void*, const void*) = run_queue_builder->cmp;
    
    while (run_queue_builder->left != run_queue_builder->last)
    {
        run_queue_builder->head = run_queue_builder->left;
        
        if (cmp(*run_queue_bulder->left, *run_queue_builder->right) <= 0) {
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
        
        if (run_queue_builder->left == run_queue_builder->last)
        {
            /***************************************************************
            * Deal with a single element run at the very tail of the input *
            * array range.                                                 *
            ***************************************************************/
            if (cmp(*(last - element_size), *last) <= 0)
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
        
        return run_queue_builder->run_queue;
    }
}

void adaptive_mergesort(void* base,
                        size_t num,
                        size_t size,
                        int (*compar)(const void*, const void*))
{
    
}
