/**CFile****************************************************************

  FileName    [listPtr.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Doubly-linked lists.]

  Synopsis    [Doubly-linked lists of generic pointers.]

  Author      []
  
  Affiliation []

  Date        [Ver. 1.0. Started - March 26, 2025.]

  Revision    [$Id: listPtr.h,v 1.00 2025/03/26 ]

***********************************************************************/
 
#ifndef ABC__misc__list__listPtr_h
#define ABC__misc__list__listPtr_h


////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <stdio.h>

ABC_NAMESPACE_HEADER_START


////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct List_Ptr_Node_t_    List_Ptr_Node_t;
typedef struct List_Ptr_t_         List_Ptr_t;
typedef struct List_Ptr_Iterator_t_ List_Ptr_Iterator_t;

// Node structure for doubly-linked list
struct List_Ptr_Node_t_ 
{
    void *              pData;        // pointer to the data
    List_Ptr_Node_t *   pNext;        // pointer to the next node
    List_Ptr_Node_t *   pPrev;        // pointer to the previous node
};

// Main list structure
struct List_Ptr_t_ 
{
    List_Ptr_Node_t *   pHead;        // pointer to the first node
    List_Ptr_Node_t *   pTail;        // pointer to the last node
    int                 nSize;        // number of nodes in the list
    List_Ptr_Node_t *   pCurItera;
};

// Iterator structure for traversing the list
struct List_Ptr_Iterator_t_
{
    List_Ptr_Node_t *   pCurrent;     // current node
    List_Ptr_t *        pList;        // reference to the list
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

// Iterators through entries
#define List_PtrForEach( Type, pList, pEntry, pIter )                                             \
    for ( List_PtrIteratorInit(pList, pIter); !List_PtrIteratorIsEnd(pIter) &&                    \
         (((pEntry) = (Type)List_PtrIteratorData(pIter)), 1); List_PtrIteratorNext(pIter) )


////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates and initializes an empty list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_t * List_PtrAlloc()
{
    List_Ptr_t * p;
    p = ABC_ALLOC( List_Ptr_t, 1 );
    if ( p == NULL )
        return NULL;
    p->pHead = NULL;
    p->pTail = NULL;
    p->nSize = 0;
    p->pCurItera = NULL; 
    return p;
}

/**Function*************************************************************

  Synopsis    [Creates a new node with given data.]

  Description [Internal function to create a new node.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrNodeAlloc( void * pData )
{
    List_Ptr_Node_t * pNode;
    pNode = ABC_ALLOC( List_Ptr_Node_t, 1 );
    if ( pNode == NULL )
        return NULL;
    pNode->pData = pData;
    pNode->pNext = NULL;
    pNode->pPrev = NULL;
    return pNode;
}

/**Function*************************************************************

  Synopsis    [Returns the first node in the list.]

  Description [Returns a pointer to the first node, not just its data.]
               
  SideEffects []

  SeeAlso     [List_PtrFirst, List_PtrLastNode]

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrFirstNode( List_Ptr_t * p )
{
    if ( p == NULL )
        return NULL;
    return p->pHead;
}

/**Function*************************************************************

  Synopsis    [Returns the last node in the list.]

  Description [Returns a pointer to the last node, not just its data.]
               
  SideEffects []

  SeeAlso     [List_PtrLast, List_PtrFirstNode]

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrLastNode( List_Ptr_t * p )
{
    if ( p == NULL )
        return NULL;
    return p->pTail;
}

/**Function*************************************************************

  Synopsis    [Frees a list node.]

  Description [Internal function to free a node.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrNodeFree( List_Ptr_Node_t * pNode )
{   
    ABC_FREE( pNode );
}

/**Function*************************************************************

  Synopsis    [Frees the list and all its nodes.]

  Description [Frees the list structure but not the data stored in it.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrFree( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode, * pNext;

    if ( p == NULL )
        return;

    // free all nodes
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        List_PtrNodeFree( pNode );
        pNode = pNext;
    }

    // free the list structure
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Frees the list, its nodes, and the data stored in them.]

  Description [Frees the list structure and the data stored in it.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrFreeData( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode, * pNext;

    if ( p == NULL )
        return;

    // free all nodes and their data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        if ( pNode->pData )
            ABC_FREE( pNode->pData );
        List_PtrNodeFree( pNode );
        pNode = pNext;
    }

    // free the list structure
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Frees the list and all its nodes using custom free function.]

  Description [Applies a custom free function to each data item.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrFreeFunc( List_Ptr_t * p, void (*pFuncFree)(void *) )
{
    List_Ptr_Node_t * pNode, * pNext;

    if ( p == NULL || pFuncFree == NULL )
        return;

    // free all nodes and their data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        if ( pNode->pData )
            pFuncFree( pNode->pData );
        List_PtrNodeFree( pNode );
        pNode = pNext;
    }

    // free the list structure
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Returns the number of elements in the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrSize( List_Ptr_t * p )
{
    if ( p == NULL )
        return 0;
    return p->nSize;
}

/**Function*************************************************************

  Synopsis    [Checks if the list is empty.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrIsEmpty( List_Ptr_t * p )
{
    if ( p == NULL )
        return 1;
    return (p->nSize == 0);
}

/**Function*************************************************************

  Synopsis    [Returns the first element of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrFirst( List_Ptr_t * p )
{
    if ( p == NULL || p->pHead == NULL )
        return NULL;
    return p->pHead->pData;
}

/**Function*************************************************************

  Synopsis    [Returns the last element of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrLast( List_Ptr_t * p )
{
    if ( p == NULL || p->pTail == NULL )
        return NULL;
    return p->pTail->pData;
}

/**Function*************************************************************

  Synopsis    [Clears all nodes from the list.]

  Description [Removes and frees all nodes but keeps the list structure.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrClear( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode, * pNext;

    if ( p == NULL )
        return;

    // free all nodes
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        List_PtrNodeFree( pNode );
        pNode = pNext;
    }

    // reset list
    p->pHead = NULL;
    p->pTail = NULL;
    p->nSize = 0;
}

/**Function*************************************************************

  Synopsis    [Clears and frees data from all nodes in the list.]

  Description [Removes and frees all nodes and their data but keeps the list.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrClearData( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode, * pNext;

    if ( p == NULL )
        return;

    // free all nodes and their data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        if ( pNode->pData )
            ABC_FREE( pNode->pData );
        List_PtrNodeFree( pNode );
        pNode = pNext;
    }

    // reset list
    p->pHead = NULL;
    p->pTail = NULL;
    p->nSize = 0;
}

/**Function*************************************************************

  Synopsis    [Adds a new element to the beginning of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrPushFront( List_Ptr_t * p, void * pData )
{
    List_Ptr_Node_t * pNode;

    if ( p == NULL )
        return NULL;

    pNode = List_PtrNodeAlloc( pData );
    if ( pNode == NULL )
        return NULL;

    // handle empty list case
    if ( p->pHead == NULL )
    {
        p->pHead = pNode;
        p->pTail = pNode;
    }
    else
    {
        pNode->pNext = p->pHead;
        p->pHead->pPrev = pNode;
        p->pHead = pNode;
    }

    p->nSize++;
    return pNode;
}

/**Function*************************************************************

  Synopsis    [Adds a new element to the end of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrPushBack( List_Ptr_t * p, void * pData )
{
    List_Ptr_Node_t * pNode;

    if ( p == NULL )
        return NULL;

    pNode = List_PtrNodeAlloc( pData );
    if ( pNode == NULL )
        return NULL;

    // handle empty list case
    if ( p->pTail == NULL )
    {
        p->pHead = pNode;
        p->pTail = pNode;
    }
    else
    {
        pNode->pPrev = p->pTail;
        p->pTail->pNext = pNode;
        p->pTail = pNode;
    }

    p->nSize++;
    return pNode;
}

/**Function*************************************************************

  Synopsis    [Removes and returns the first element of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrPopFront( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode;
    void * pData;

    if ( p == NULL || p->pHead == NULL )
        return NULL;

    pNode = p->pHead;
    pData = pNode->pData;

    // update list head
    p->pHead = pNode->pNext;
    
    // handle case when list becomes empty
    if ( p->pHead == NULL )
        p->pTail = NULL;
    else
        p->pHead->pPrev = NULL;

    List_PtrNodeFree( pNode );
    p->nSize--;
    
    return pData;
}

/**Function*************************************************************

  Synopsis    [Removes and returns the last element of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrPopBack( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode;
    void * pData;

    if ( p == NULL || p->pTail == NULL )
        return NULL;

    pNode = p->pTail;
    pData = pNode->pData;

    // update list tail
    p->pTail = pNode->pPrev;
    
    // handle case when list becomes empty
    if ( p->pTail == NULL )
        p->pHead = NULL;
    else
        p->pTail->pNext = NULL;

    List_PtrNodeFree( pNode );
    p->nSize--;
    
    return pData;
}

/**Function*************************************************************

  Synopsis    [Inserts a new element after the specified node.]

  Description [Internal function used by other insertion methods.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrInsertAfter( List_Ptr_t * p, List_Ptr_Node_t * pNode, void * pData )
{
    List_Ptr_Node_t * pNewNode;

    if ( p == NULL || pNode == NULL )
        return NULL;

    pNewNode = List_PtrNodeAlloc( pData );
    if ( pNewNode == NULL )
        return NULL;

    pNewNode->pNext = pNode->pNext;
    pNewNode->pPrev = pNode;
    
    if ( pNode->pNext != NULL )
        pNode->pNext->pPrev = pNewNode;
    else
        p->pTail = pNewNode;  // Update tail if inserting at the end
    
    pNode->pNext = pNewNode;
    p->nSize++;
    
    return pNewNode;
}

/**Function*************************************************************

  Synopsis    [Inserts a new element before the specified node.]

  Description [Internal function used by other insertion methods.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrInsertBefore( List_Ptr_t * p, List_Ptr_Node_t * pNode, void * pData )
{
    List_Ptr_Node_t * pNewNode;

    if ( p == NULL || pNode == NULL )
        return NULL;

    pNewNode = List_PtrNodeAlloc( pData );
    if ( pNewNode == NULL )
        return NULL;

    pNewNode->pNext = pNode;
    pNewNode->pPrev = pNode->pPrev;
    
    if ( pNode->pPrev != NULL )
        pNode->pPrev->pNext = pNewNode;
    else
        p->pHead = pNewNode;  // Update head if inserting at the beginning
    
    pNode->pPrev = pNewNode;
    p->nSize++;
    
    return pNewNode;
}

/**Function*************************************************************

  Synopsis    [Inserts a new element at the specified position.]

  Description [Position is 0-based. If position is out of bounds, it inserts
  at the beginning (if position < 0) or at the end (if position >= size).]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrInsertAt( List_Ptr_t * p, int Position, void * pData )
{
    List_Ptr_Node_t * pNode;
    int i;

    if ( p == NULL )
        return NULL;
    
    // Handle special cases
    if ( Position <= 0 )
        return List_PtrPushFront( p, pData );
    
    if ( Position >= p->nSize )
        return List_PtrPushBack( p, pData );
    
    // Find the node at the specified position
    if ( Position <= p->nSize / 2 )
    {
        // Start from the beginning if position is in the first half
        pNode = p->pHead;
        for ( i = 0; i < Position; i++ )
            pNode = pNode->pNext;
    }
    else
    {
        // Start from the end if position is in the second half
        pNode = p->pTail;
        for ( i = p->nSize - 1; i > Position; i-- )
            pNode = pNode->pPrev;
    }
    
    // Insert before the found node
    return List_PtrInsertBefore( p, pNode, pData );
}

/**Function*************************************************************

  Synopsis    [Removes the specified node from the list.]

  Description [Internal function used by other removal methods.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrRemoveNode( List_Ptr_t * p, List_Ptr_Node_t * pNode )
{
    void * pData;

    if ( p == NULL || pNode == NULL )
        return NULL;

    pData = pNode->pData;
    
    // Update adjacent nodes
    if ( pNode->pPrev != NULL )
        pNode->pPrev->pNext = pNode->pNext;
    else
        p->pHead = pNode->pNext;  // Update head if removing first node
        
    if ( pNode->pNext != NULL )
        pNode->pNext->pPrev = pNode->pPrev;
    else
        p->pTail = pNode->pPrev;  // Update tail if removing last node
    

    List_PtrNodeFree( pNode );
    p->nSize--;
    
    return pData;
}

/**Function*************************************************************

  Synopsis    [Removes the element at the specified position.]

  Description [Position is 0-based. Returns NULL if position is out of bounds.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrRemoveAt( List_Ptr_t * p, int Position )
{
    List_Ptr_Node_t * pNode;
    int i;

    if ( p == NULL || Position < 0 || Position >= p->nSize )
        return NULL;
    
    // Special cases for first and last elements
    if ( Position == 0 )
        return List_PtrPopFront( p );
    
    if ( Position == p->nSize - 1 )
        return List_PtrPopBack( p );
    
    // Find the node at the specified position
    if ( Position <= p->nSize / 2 )
    {
        // Start from the beginning if position is in the first half
        pNode = p->pHead;
        for ( i = 0; i < Position; i++ )
            pNode = pNode->pNext;
    }
    else
    {
        // Start from the end if position is in the second half
        pNode = p->pTail;
        for ( i = p->nSize - 1; i > Position; i-- )
            pNode = pNode->pPrev;
    }
    
    // Remove the found node
    return List_PtrRemoveNode( p, pNode );
}

/**Function*************************************************************

  Synopsis    [Removes the first occurrence of the specified data.]

  Description [Searches for the data and removes the first matching node.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrRemove( List_Ptr_t * p, void * pData )
{
    List_Ptr_Node_t * pNode;

    if ( p == NULL )
        return 0;
    
    // Search for the node with the specified data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        if ( pNode->pData == pData )
        {
            List_PtrRemoveNode( p, pNode );
            return 1;
        }
        pNode = pNode->pNext;
    }
    
    return 0;  // Data not found
}

/**Function*************************************************************

  Synopsis    [Removes all occurrences of the specified data.]

  Description [Searches for the data and removes all matching nodes.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrRemoveAll( List_Ptr_t * p, void * pData )
{
    List_Ptr_Node_t * pNode, * pNext;
    int Count = 0;

    if ( p == NULL )
        return 0;
    
    // Search for and remove all nodes with the specified data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;  // Save next pointer before potentially removing
        
        if ( pNode->pData == pData )
        {
            List_PtrRemoveNode( p, pNode );
            Count++;
        }
        
        pNode = pNext;
    }
    
    return Count;  // Return number of nodes removed
}

/**Function*************************************************************

  Synopsis    [Finds the first occurrence of the specified data.]

  Description [Returns position (0-based) of the data or -1 if not found.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrFind( List_Ptr_t * p, void * pData )
{
    List_Ptr_Node_t * pNode;
    int Position = 0;

    if ( p == NULL )
        return -1;
    
    // Search for the node with the specified data
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        if ( pNode->pData == pData )
            return Position;
        
        pNode = pNode->pNext;
        Position++;
    }
    
    return -1;  // Data not found
}

/**Function*************************************************************

  Synopsis    [Checks if the list contains the specified data.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrContains( List_Ptr_t * p, void * pData )
{
    return List_PtrFind( p, pData ) >= 0;
}

/**Function*************************************************************

  Synopsis    [Gets the element at the specified position.]

  Description [Position is 0-based. Returns NULL if position is out of bounds.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrGetAt( List_Ptr_t * p, int Position )
{
    List_Ptr_Node_t * pNode;
    int i;

    if ( p == NULL || Position < 0 || Position >= p->nSize )
        return NULL;
    
    // Special cases for first and last elements
    if ( Position == 0 )
        return List_PtrFirst( p );
    
    if ( Position == p->nSize - 1 )
        return List_PtrLast( p );
    
    // Find the node at the specified position
    if ( Position <= p->nSize / 2 )
    {
        // Start from the beginning if position is in the first half
        pNode = p->pHead;
        for ( i = 0; i < Position; i++ )
            pNode = pNode->pNext;
    }
    else
    {
        // Start from the end if position is in the second half
        pNode = p->pTail;
        for ( i = p->nSize - 1; i > Position; i-- )
            pNode = pNode->pPrev;
    }
    
    return pNode->pData;
}

/**Function*************************************************************

  Synopsis    [Sets the element at the specified position.]

  Description [Position is 0-based. Returns old data or NULL if error.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrSetAt( List_Ptr_t * p, int Position, void * pData )
{
    List_Ptr_Node_t * pNode;
    void * pOldData;
    int i;

    if ( p == NULL || Position < 0 || Position >= p->nSize )
        return NULL;
    
    // Find the node at the specified position
    if ( Position <= p->nSize / 2 )
    {
        // Start from the beginning if position is in the first half
        pNode = p->pHead;
        for ( i = 0; i < Position; i++ )
            pNode = pNode->pNext;
    }
    else
    {
        // Start from the end if position is in the second half
        pNode = p->pTail;
        for ( i = p->nSize - 1; i > Position; i-- )
            pNode = pNode->pPrev;
    }
    
    // Replace data
    pOldData = pNode->pData;
    pNode->pData = pData;
    
    return pOldData;
}


/**Function*************************************************************

  Synopsis    [Creates a duplicate of a node and maintains its list relationships.]

  Description [Creates a new node with the same data and connects it to the 
               same adjacent nodes as the original node. The original node is 
               effectively replaced in the list by the new node.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrDupNodeEmpty( List_Ptr_t * p, List_Ptr_Node_t * pNode )
{
    List_Ptr_Node_t * pNewNode;

    if ( p == NULL || pNode == NULL )
        return NULL;
    
    // Allocate and initialize the new node with the same data
    pNewNode = List_PtrNodeAlloc( NULL );
    if ( pNewNode == NULL )
        return NULL;
    
    // Connect the new node to the same adjacent nodes
    pNewNode->pNext = pNode->pNext;
    pNewNode->pPrev = pNode->pPrev;
    
    // Update adjacent nodes to point to the new node
    if ( pNewNode->pPrev != NULL )
        pNewNode->pPrev->pNext = pNewNode;
    else
        p->pHead = pNewNode;  // Update head if duplicating the first node
        
    if ( pNewNode->pNext != NULL )
        pNewNode->pNext->pPrev = pNewNode;
    else
        p->pTail = pNewNode;  // Update tail if duplicating the last node
    
    return pNewNode;
}

/**Function*************************************************************

  Synopsis    [Creates a duplicate of the list.]

  Description [Creates a new list with the same elements.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_t * List_PtrDup( List_Ptr_t * p )
{
    List_Ptr_t * pNew;
    List_Ptr_Node_t * pNode;

    if ( p == NULL )
        return NULL;
    
    pNew = List_PtrAlloc();
    if ( pNew == NULL )
        return NULL;
    
    // Copy all elements
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        if ( !List_PtrPushBack( pNew, pNode->pData ) )
        {
            List_PtrFree( pNew );
            return NULL;
        }
        pNode = pNode->pNext;
    }
    
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Reverses the order of elements in the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrReverse( List_Ptr_t * p )
{
    List_Ptr_Node_t * pNode, * pNext, * pTemp;

    if ( p == NULL || p->nSize <= 1 )
        return;
    
    // Swap next and prev pointers for all nodes
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pNext = pNode->pNext;
        
        // Swap next and prev pointers
        pTemp = pNode->pNext;
        pNode->pNext = pNode->pPrev;
        pNode->pPrev = pTemp;
        
        pNode = pNext;
    }
    
    // Swap head and tail pointers
    pTemp = p->pHead;
    p->pHead = p->pTail;
    p->pTail = pTemp;
}

/**Function*************************************************************

  Synopsis    [Initializes an iterator for the list.]

  Description [Sets the iterator to the first element.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrIteratorInit( List_Ptr_t * pList, List_Ptr_Iterator_t * pIter )
{
    if ( pList == NULL || pIter == NULL )
        return;
    
    pIter->pList = pList;
    pIter->pCurrent = pList->pHead;
}

/**Function*************************************************************

  Synopsis    [Initializes an iterator for the list at the last element.]

  Description [Sets the iterator to the last element.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrIteratorInitLast( List_Ptr_t * pList, List_Ptr_Iterator_t * pIter )
{
    if ( pList == NULL || pIter == NULL )
        return;
    
    pIter->pList = pList;
    pIter->pCurrent = pList->pTail;
}

/**Function*************************************************************

  Synopsis    [Moves the iterator to the next element.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrIteratorNext( List_Ptr_Iterator_t * pIter )
{
    if ( pIter == NULL || pIter->pCurrent == NULL )
        return;
    
    pIter->pCurrent = pIter->pCurrent->pNext;
}

/**Function*************************************************************

  Synopsis    [Moves the iterator to the previous element.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrIteratorPrev( List_Ptr_Iterator_t * pIter )
{
    if ( pIter == NULL || pIter->pCurrent == NULL )
        return;
    
    pIter->pCurrent = pIter->pCurrent->pPrev;
}

/**Function*************************************************************

  Synopsis    [Checks if the iterator has reached the end of the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrIteratorIsEnd( List_Ptr_Iterator_t * pIter )
{
    if ( pIter == NULL )
        return 1;
    
    return pIter->pCurrent == NULL;
}

/**Function*************************************************************

  Synopsis    [Checks if the iterator has reached the beginning of the list.]

  Description [Used primarily for reverse iteration.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrIteratorIsBOL( List_Ptr_Iterator_t * pIter )
{
    if ( pIter == NULL )
        return 1;
    
    return pIter->pCurrent == NULL;
}

/**Function*************************************************************

  Synopsis    [Gets the data at the current iterator position.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrIteratorData( List_Ptr_Iterator_t * pIter )
{
    if ( pIter == NULL || pIter->pCurrent == NULL )
        return NULL;
    
    return pIter->pCurrent->pData;
}

/**Function*************************************************************

  Synopsis    [Inserts data after the current iterator position.]

  Description [Does not change the iterator position.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrIteratorInsertAfter( List_Ptr_Iterator_t * pIter, void * pData )
{
    if ( pIter == NULL || pIter->pList == NULL )
        return NULL;
    
    if ( pIter->pCurrent == NULL )
        return List_PtrPushFront( pIter->pList, pData );
    
    return List_PtrInsertAfter( pIter->pList, pIter->pCurrent, pData );
}

/**Function*************************************************************

  Synopsis    [Inserts data before the current iterator position.]

  Description [Does not change the iterator position.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_Node_t * List_PtrIteratorInsertBefore( List_Ptr_Iterator_t * pIter, void * pData )
{
    if ( pIter == NULL || pIter->pList == NULL )
        return NULL;
    
    if ( pIter->pCurrent == NULL )
        return List_PtrPushBack( pIter->pList, pData );
    
    return List_PtrInsertBefore( pIter->pList, pIter->pCurrent, pData );
}

/**Function*************************************************************

  Synopsis    [Removes the element at the current iterator position.]

  Description [Advances the iterator to the next position.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void * List_PtrIteratorRemove( List_Ptr_Iterator_t * pIter )
{
    List_Ptr_Node_t * pNext;
    void * pData;

    if ( pIter == NULL || pIter->pList == NULL || pIter->pCurrent == NULL )
        return NULL;
    
    // Save next pointer before removing
    pNext = pIter->pCurrent->pNext;
    
    // Remove node and get data
    pData = List_PtrRemoveNode( pIter->pList, pIter->pCurrent );
    
    // Update iterator to point to next node
    pIter->pCurrent = pNext;
    
    return pData;
}

/**Function*************************************************************

  Synopsis    [Returns the estimated memory usage of the list in bytes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline double List_PtrMemory( List_Ptr_t * p )
{
    if ( p == NULL )
        return 0.0;
    
    return sizeof(List_Ptr_t) + sizeof(List_Ptr_Node_t) * (size_t)p->nSize;
}

/**Function*************************************************************

  Synopsis    [Swaps the contents of two lists.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrSwap( List_Ptr_t * p1, List_Ptr_t * p2 )
{
    List_Ptr_Node_t * pHeadTemp, * pTailTemp;
    int nSizeTemp;
    
    if ( p1 == NULL || p2 == NULL || p1 == p2 )
        return;
    
    // Swap head pointers
    pHeadTemp = p1->pHead;
    p1->pHead = p2->pHead;
    p2->pHead = pHeadTemp;
    
    // Swap tail pointers
    pTailTemp = p1->pTail;
    p1->pTail = p2->pTail;
    p2->pTail = pTailTemp;
    
    // Swap sizes
    nSizeTemp = p1->nSize;
    p1->nSize = p2->nSize;
    p2->nSize = nSizeTemp;
}

/**Function*************************************************************

  Synopsis    [Creates a list from an array of pointers.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_t * List_PtrFromArray( void ** pArray, int nSize )
{
    List_Ptr_t * p;
    int i;
    
    if ( pArray == NULL || nSize <= 0 )
        return NULL;
    
    p = List_PtrAlloc();
    if ( p == NULL )
        return NULL;
    
    for ( i = 0; i < nSize; i++ )
    {
        if ( !List_PtrPushBack( p, pArray[i] ) )
        {
            List_PtrFree( p );
            return NULL;
        }
    }
    
    return p;
}

/**Function*************************************************************

  Synopsis    [Converts a list to an array of pointers.]

  Description [Returns an array that must be freed by the caller.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void ** List_PtrToArray( List_Ptr_t * p )
{
    void ** pArray;
    List_Ptr_Node_t * pNode;
    int i;
    
    if ( p == NULL || p->nSize == 0 )
        return NULL;
    
    pArray = ABC_ALLOC( void *, p->nSize );
    if ( pArray == NULL )
        return NULL;
    
    pNode = p->pHead;
    for ( i = 0; i < p->nSize; i++ )
    {
        if ( pNode == NULL )
        {
            ABC_FREE( pArray );
            return NULL;
        }
        
        pArray[i] = pNode->pData;
        pNode = pNode->pNext;
    }
    
    return pArray;
}

/**Function*************************************************************

  Synopsis    [Concatenates two lists.]

  Description [Appends all elements from the second list to the first list.
  The second list remains unchanged.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int List_PtrConcat( List_Ptr_t * pDest, List_Ptr_t * pSrc )
{
    List_Ptr_Node_t * pNode;
    
    if ( pDest == NULL || pSrc == NULL )
        return 0;
    
    if ( pSrc->nSize == 0 )
        return 1;  // Nothing to append
    
    // Append all nodes from source list
    pNode = pSrc->pHead;
    while ( pNode != NULL )
    {
        if ( !List_PtrPushBack( pDest, pNode->pData ) )
            return 0;
        
        pNode = pNode->pNext;
    }
    
    return 1;
}

/**Function*************************************************************

  Synopsis    [Applies a function to each element in the list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void List_PtrForEachApply( List_Ptr_t * p, void (*pFunc)(void *) )
{
    List_Ptr_Node_t * pNode;
    
    if ( p == NULL || pFunc == NULL )
        return;
    
    pNode = p->pHead;
    while ( pNode != NULL )
    {
        pFunc( pNode->pData );
        pNode = pNode->pNext;
    }
}

/**Function*************************************************************

  Synopsis    [Thread-safe version that returns a copy of the current list.]

  Description [Creates a new list with a copy of all elements from the current list.
  Can be used in multi-threaded environments where the original list might be modified
  by another thread while iterating.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline List_Ptr_t * List_PtrSnapshot( List_Ptr_t * p )
{
    return List_PtrDup( p );
}

ABC_NAMESPACE_HEADER_END

#endif // ABC__misc__list__listPtr_h