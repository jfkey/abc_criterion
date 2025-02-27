/**CFile****************************************************************

  FileName    [pQue.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Priority queue implementation.]

  Synopsis    [A priority queue that stores elements with associated priorities.]

  Author      [Junfeng Liu]

  Affiliation []

  Date        [Ver. 1.0. Started - October 20, 2023.]

***********************************************************************/

#ifndef ABC__misc__pQue_h
#define ABC__misc__pQue_h

#include <stdio.h>
#include <assert.h>

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct P_Que_t_  P_Que_t;
struct P_Que_t_ 
{
    int             nCap;          // Capacity of the queue
    int             nSize;         // Current size of the queue
    void **         pHeap;         // Pointer to the heap of elements
    float *         pPriorities;   // Pointer to the array of priorities
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates a priority queue with the given capacity.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline P_Que_t * P_QueAlloc(int nCap)
{
    P_Que_t * p;
    p = (P_Que_t *)malloc(sizeof(P_Que_t));
    if (nCap < 16)
        nCap = 16;
    p->nSize = 0;
    p->nCap = nCap;
    p->pHeap = (void **)malloc(nCap * sizeof(void *));
    p->pPriorities = (float *)malloc(nCap * sizeof(float));
    return p;
}

/**Function*************************************************************

  Synopsis    [Frees the priority queue.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/    
static inline void P_QueFree(P_Que_t * p)
{
    free(p->pHeap);
    free(p->pPriorities);
    free(p);
}

/**Function*************************************************************

  Synopsis    [Returns the size of the priority queue.]

  Description []
               
  SideEffects []    

  SeeAlso     []

***********************************************************************/
static inline int P_QueSize(P_Que_t * p)
{
    return p->nSize;
}

/**Function*************************************************************

  Synopsis    [Pushes an element into the priority queue.]

  Description []
               
  SideEffects []        

  SeeAlso     []

***********************************************************************/
static inline void P_QuePush(P_Que_t * p, void *data, float priority)
{
    if (p->nSize >= p->nCap) {
        p->nCap *= 2;
        p->pHeap = (void **)realloc(p->pHeap, p->nCap * sizeof(void *));
        p->pPriorities = (float *)realloc(p->pPriorities, p->nCap * sizeof(float));
    }
    int i = p->nSize++;
    p->pHeap[i] = data;
    p->pPriorities[i] = priority;

    // Move up to maintain heap property
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (p->pPriorities[i] >= p->pPriorities[parent]) break;
        // Swap
        void *tempData = p->pHeap[i];
        float tempPriority = p->pPriorities[i];
        p->pHeap[i] = p->pHeap[parent];
        p->pPriorities[i] = p->pPriorities[parent];
        p->pHeap[parent] = tempData;
        p->pPriorities[parent] = tempPriority;
        i = parent;
    }
}

/**Function*************************************************************

  Synopsis    [Pops an element from the priority queue.]

  Description []            
               
  SideEffects []        

  SeeAlso     []

***********************************************************************/
static inline void * P_QuePop(P_Que_t * p)
{
    if (p->nSize == 0) return NULL;

    void *data = p->pHeap[0];
    p->pHeap[0] = p->pHeap[--p->nSize];
    p->pPriorities[0] = p->pPriorities[p->nSize];

    // Move down to maintain heap property
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < p->nSize && p->pPriorities[left] < p->pPriorities[smallest]) {
            smallest = left;
        }
        if (right < p->nSize && p->pPriorities[right] < p->pPriorities[smallest]) {
            smallest = right;
        }
        if (smallest == i) break;

        // Swap
        void *tempData = p->pHeap[i];
        float tempPriority = p->pPriorities[i];
        p->pHeap[i] = p->pHeap[smallest];
        p->pPriorities[i] = p->pPriorities[smallest];
        p->pHeap[smallest] = tempData;
        p->pPriorities[smallest] = tempPriority;
        i = smallest;
    }

    return data;
}

/**Function*************************************************************

  Synopsis    [Returns the top element of the priority queue.]

  Description []    
               
  SideEffects []        

  SeeAlso     []

***********************************************************************/    
static inline void * P_QueTop(P_Que_t * p)
{
    return p->nSize > 0 ? p->pHeap[0] : NULL;
}

/**Function*************************************************************

  Synopsis    [Returns the top element of the priority queue.]

  Description []        
               
  SideEffects []        

  SeeAlso     []

***********************************************************************/    
static inline float P_QueTopPriority(P_Que_t * p)
{
    return p->nSize > 0 ? p->pPriorities[0] : -1.0f; // Assuming negative priority isn't valid
}

ABC_NAMESPACE_HEADER_END

#endif