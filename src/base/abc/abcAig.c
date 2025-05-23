/**CFile****************************************************************

  FileName    [abcAig.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Simple structural hashing package.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcAig.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "abc.h" 

ABC_NAMESPACE_IMPL_START

abctime global_update_time = 0;         // do not use 
// abctime global_traverse_time = 0; 
abctime global_time = 0; 
abctime global_resynthesis_time = 0; 
abctime global_cut_time = 0; 
abctime global_aig_update_time = 0;     // maintain order, level /reverse level computation 
abctime global_aig_converter_time = 0;      
long int global_level_updates = 0;
long int global_reverse_updates = 0;
long int global_node_rewritten = 0; 
long int global_reorder_nodes = 0;


 

/*
    AIG is an And-Inv Graph with structural hashing.
    It is always structurally hashed. It means that at any time:
    - for each AND gate, there are no other AND gates with the same children
    - the constants are propagated
    - there is no single-input nodes (inverters/buffers)
    Additionally the following invariants are satisfied:
    - there are no dangling nodes (the nodes without fanout)
    - the level of each AND gate reflects the levels of this fanins
    - the EXOR-status of each node is up-to-date
    - the AND nodes are in the topological order
    - the constant 1 node has always number 0 in the object list
    The operations that are performed on AIGs:
    - building new nodes (Abc_AigAnd)
    - performing elementary Boolean operations (Abc_AigOr, Abc_AigXor, etc)
    - replacing one node by another (Abc_AigReplace)
    - propagating constants (Abc_AigReplace)
    When AIG is duplicated, the new graph is structurally hashed too.
    If this repeated hashing leads to fewer nodes, it means the original
    AIG was not strictly hashed (one of the conditions above is violated).
*/

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// the simple AIG manager
struct Abc_Aig_t_
{
    Abc_Ntk_t *       pNtkAig;           // the AIG network
    Abc_Obj_t *       pConst1;           // the constant 1 object (not a node!)
    Abc_Obj_t **      pBins;             // the table bins
    int               nBins;             // the size of the table
    int               nEntries;          // the total number of entries in the table
    Vec_Ptr_t *       vNodes;            // the temporary array of nodes
    Vec_Ptr_t *       vStackReplaceOld;  // the nodes to be replaced
    Vec_Ptr_t *       vStackReplaceNew;  // the nodes to be used for replacement
    Vec_Vec_t *       vLevels;           // the nodes to be updated
    Vec_Vec_t *       vLevelsR;          // the nodes to be updated

    P_Que_t *         qLevels;            // the queue of nodes to be updated
    P_Que_t *         qLevelsR;           // the queue of nodes to be updated    
    Vec_Ptr_t *       vTopoAff;           // the queue of nodes whose topological order are affected

    int               nLevelMin;         // the minimum level to be updated
    
    Vec_Ptr_t *       vAddedCells;       // the added nodes
    Vec_Ptr_t *       vUpdatedNets;      // the nodes whose fanouts have changed

    // Heap<Abc_Obj_t *>  vHeap;            // the heap of nodes to be updated 
    // Heap<Abc_Obj_t *>  vHeapR;            // the heap of nodes to be updated 
    
    List_Ptr_t *       oList;            // the linked list of nodes


    int               nStrash0;
    int               nStrash1;
    int               nStrash5;
    int               nStrash2;
};

// iterators through the entries in the linked lists of nodes
#define Abc_AigBinForEachEntry( pBin, pEnt )                   \
    for ( pEnt = pBin;                                         \
          pEnt;                                                \
          pEnt = pEnt->pNext )
#define Abc_AigBinForEachEntrySafe( pBin, pEnt, pEnt2 )        \
    for ( pEnt = pBin,                                         \
          pEnt2 = pEnt? pEnt->pNext: NULL;                     \
          pEnt;                                                \
          pEnt = pEnt2,                                        \
          pEnt2 = pEnt? pEnt->pNext: NULL )

// hash key for the structural hash table
//static inline unsigned Abc_HashKey2( Abc_Obj_t * p0, Abc_Obj_t * p1, int TableSize ) { return ((unsigned)(p0) + (unsigned)(p1) * 12582917) % TableSize; }
//static inline unsigned Abc_HashKey2( Abc_Obj_t * p0, Abc_Obj_t * p1, int TableSize ) { return ((unsigned)((a)->Id + (b)->Id) * ((a)->Id + (b)->Id + 1) / 2) % TableSize; }

// hashing the node
static unsigned Abc_HashKey2( Abc_Obj_t * p0, Abc_Obj_t * p1, int TableSize ) 
{
    unsigned Key = 0;
    Key ^= Abc_ObjRegular(p0)->Id * 7937;
    Key ^= Abc_ObjRegular(p1)->Id * 2971;
    Key ^= Abc_ObjIsComplement(p0) * 911;
    Key ^= Abc_ObjIsComplement(p1) * 353;
    return Key % TableSize;
}

// structural hash table procedures
static Abc_Obj_t * Abc_AigAndCreate( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 );
static Abc_Obj_t * Abc_AigAndCreateFrom( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1, Abc_Obj_t * pAnd );
static void        Abc_AigAndDelete( Abc_Aig_t * pMan, Abc_Obj_t * pThis );
static void        Abc_AigResize( Abc_Aig_t * pMan );
// incremental AIG procedures
static void        Abc_AigReplace_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel );
static void        Abc_AigUpdateLevel_int( Abc_Aig_t * pMan );
static void        Abc_AigUpdateLevelR_int( Abc_Aig_t * pMan );
static void        Abc_AigRemoveFromLevelStructure( Vec_Vec_t * vStruct, Abc_Obj_t * pNode );
static void        Abc_AigRemoveFromLevelStructureR( Vec_Vec_t * vStruct, Abc_Obj_t * pNode );

static void        Abc_AigReplaceInc_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel );
static void        Abc_AigReplaceIncLevelUpdate_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel );


// new graph incremental algorithm for reverse level updates
static void        Abc_AigUpdateLevelR_new( Abc_Aig_t * pMan, Abc_Obj_t * pOld);
static void        Abc_AigUpdateLevelInc_int( Abc_Aig_t * pMan );
static void        Abc_AigUpdateLevelIncR_int( Abc_Aig_t * pMan );
static void        Abc_AigAndDeleteInc( Abc_Aig_t * pMan, Abc_Obj_t * pThis );
static void        Abc_AigReplaceFindAff_rec( Abc_Aig_t * pMan, Abc_Obj_t * pFrom);
 

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates the local AIG manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Aig_t * Abc_AigAlloc( Abc_Ntk_t * pNtkAig )
{
    Abc_Aig_t * pMan;
    // start the manager
    pMan = ABC_ALLOC( Abc_Aig_t, 1 );
    memset( pMan, 0, sizeof(Abc_Aig_t) );
    // allocate the table
    pMan->nBins    = Abc_PrimeCudd( 10000 );
    pMan->pBins    = ABC_ALLOC( Abc_Obj_t *, pMan->nBins );
    memset( pMan->pBins, 0, sizeof(Abc_Obj_t *) * pMan->nBins );
    pMan->vNodes   = Vec_PtrAlloc( 100 );
    pMan->vLevels  = Vec_VecAlloc( 100 );
    pMan->vLevelsR = Vec_VecAlloc( 100 );
    pMan->vStackReplaceOld = Vec_PtrAlloc( 100 );
    pMan->vStackReplaceNew = Vec_PtrAlloc( 100 );

    pMan->qLevels = P_QueAlloc( 100 );
    pMan->qLevelsR = P_QueAlloc( 100 );
    pMan->vTopoAff = Vec_PtrAlloc( 100 );
    pMan->nLevelMin = ABC_INFINITY;

    pMan->oList = List_PtrAlloc();

    // create the constant node
    assert( pNtkAig->vObjs->nSize == 0 );
    pMan->pConst1 = Abc_NtkCreateObj( pNtkAig, ABC_OBJ_NODE );
    pMan->pConst1->Type = ABC_OBJ_CONST1;
    pMan->pConst1->fPhase = 1;
    pNtkAig->nObjCounts[ABC_OBJ_NODE]--;
    // save the current network
    pMan->pNtkAig = pNtkAig;
    return pMan;
}

/**Function*************************************************************

  Synopsis    [Deallocates the local AIG manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigFree( Abc_Aig_t * pMan )
{
    assert( Vec_PtrSize( pMan->vStackReplaceOld ) == 0 );
    assert( Vec_PtrSize( pMan->vStackReplaceNew ) == 0 );
    // free the table
    if ( pMan->vAddedCells )
        Vec_PtrFree( pMan->vAddedCells );
    if ( pMan->vUpdatedNets )
        Vec_PtrFree( pMan->vUpdatedNets );
    Vec_VecFree( pMan->vLevels );
    Vec_VecFree( pMan->vLevelsR );
    Vec_PtrFree( pMan->vStackReplaceOld );
    Vec_PtrFree( pMan->vStackReplaceNew );
    Vec_PtrFree( pMan->vNodes );
    // free the queues  
    P_QueFree( pMan->qLevels );
    P_QueFree( pMan->qLevelsR );
    Vec_PtrFree( pMan->vTopoAff );
    List_PtrFree( pMan->oList );
    ABC_FREE( pMan->pBins );
    ABC_FREE( pMan );
}

/**Function*************************************************************

  Synopsis    [Returns the number of dangling nodes removed.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigCleanup( Abc_Aig_t * pMan )
{
    Vec_Ptr_t * vDangles;
    Abc_Obj_t * pAnd;
    int i, nNodesOld;
//    printf( "Strash0 = %d.  Strash1 = %d.  Strash100 = %d.  StrashM = %d.\n", 
//        pMan->nStrash0, pMan->nStrash1, pMan->nStrash5, pMan->nStrash2 );
    nNodesOld = pMan->nEntries;
    // collect the AND nodes that do not fanout
    vDangles = Vec_PtrAlloc( 100 );
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntry( pMan->pBins[i], pAnd )
            if ( Abc_ObjFanoutNum(pAnd) == 0 )
                Vec_PtrPush( vDangles, pAnd );
    // process the dangling nodes and their MFFCs
    Vec_PtrForEachEntry( Abc_Obj_t *, vDangles, pAnd, i )
        Abc_AigDeleteNode( pMan, pAnd );
    Vec_PtrFree( vDangles );
    return nNodesOld - pMan->nEntries;
}

/**Function*************************************************************

  Synopsis    [Makes sure that every node in the table is in the network and vice versa.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigCheck( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pObj, * pAnd;
    int i, nFanins, Counter;
    Abc_NtkForEachNode( pMan->pNtkAig, pObj, i )
    {
        nFanins = Abc_ObjFaninNum(pObj);
        if ( nFanins == 0 )
        {
            if ( !Abc_AigNodeIsConst(pObj) )
            {
                printf( "Abc_AigCheck: The AIG has non-standard constant nodes.\n" );
                return 0;
            }
            continue;
        }
        if ( nFanins == 1 )
        {
            printf( "Abc_AigCheck: The AIG has single input nodes.\n" );
            return 0;
        }
        if ( nFanins > 2 )
        {
            printf( "Abc_AigCheck: The AIG has non-standard nodes.\n" );
            return 0;
        }
        if ( pObj->Level != 1 + (unsigned)Abc_MaxInt( Abc_ObjFanin0(pObj)->Level, Abc_ObjFanin1(pObj)->Level ) )
            printf( "Abc_AigCheck: Node \"%s\" has level that does not agree with the fanin levels.\n", Abc_ObjName(pObj) );
        pAnd = Abc_AigAndLookup( pMan, Abc_ObjChild0(pObj), Abc_ObjChild1(pObj) );
        if ( pAnd != pObj )
            printf( "Abc_AigCheck: Node \"%s\" is not in the structural hashing table.\n", Abc_ObjName(pObj) );
    }
    // count the number of nodes in the table
    Counter = 0;
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntry( pMan->pBins[i], pAnd )
            Counter++;
    if ( Counter != Abc_NtkNodeNum(pMan->pNtkAig) )
    {
        printf( "Abc_AigCheck: The number of nodes in the structural hashing table is wrong.\n" );
        return 0;
    }
    // if the node is a choice node, nodes in its class should not have fanouts
    Abc_NtkForEachNode( pMan->pNtkAig, pObj, i )
        if ( Abc_AigNodeIsChoice(pObj) )
            for ( pAnd = (Abc_Obj_t *)pObj->pData; pAnd; pAnd = (Abc_Obj_t *)pAnd->pData )
                if ( Abc_ObjFanoutNum(pAnd) > 0 )
                {
                    printf( "Abc_AigCheck: Representative %s", Abc_ObjName(pAnd) );
                    printf( " of choice node %s has %d fanouts.\n", Abc_ObjName(pObj), Abc_ObjFanoutNum(pAnd) );
                    return 0;
                }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Computes the number of logic levels not counting PIs/POs.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigLevel( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pNode;
    int i, LevelsMax;
    assert( Abc_NtkIsStrash(pNtk) );
    if ( pNtk->nBarBufs )
        return Abc_NtkLevel( pNtk );
    // perform the traversal
    LevelsMax = 0;
    Abc_NtkForEachCo( pNtk, pNode, i )
        if ( LevelsMax < (int)Abc_ObjFanin0(pNode)->Level )
            LevelsMax = (int)Abc_ObjFanin0(pNode)->Level;
    return LevelsMax;
}

/**Function*************************************************************

  Synopsis    [Return the order of aig nodes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/

List_Ptr_t * Abc_AigGetOList(Abc_Aig_t * pMan){
    return pMan->oList;
}


/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigAndCreate( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 )
{
    Abc_Obj_t * pAnd;
    unsigned Key;
    // check if it is a good time for table resizing
    if ( pMan->nEntries > 2 * pMan->nBins )
        Abc_AigResize( pMan );
    // order the arguments
    if ( Abc_ObjRegular(p0)->Id > Abc_ObjRegular(p1)->Id )
        pAnd = p0, p0 = p1, p1 = pAnd;
    // create the new node
    pAnd = Abc_NtkCreateNode( pMan->pNtkAig );
    Abc_ObjAddFanin( pAnd, p0 );
    Abc_ObjAddFanin( pAnd, p1 );
    // set the level of the new node
    pAnd->Level  = 1 + Abc_MaxInt( Abc_ObjRegular(p0)->Level, Abc_ObjRegular(p1)->Level ); 
    pAnd->fExor  = Abc_NodeIsExorType(pAnd);
    pAnd->fPhase = (Abc_ObjIsComplement(p0) ^ Abc_ObjRegular(p0)->fPhase) & (Abc_ObjIsComplement(p1) ^ Abc_ObjRegular(p1)->fPhase);
    // add the node to the corresponding linked list in the table
    Key = Abc_HashKey2( p0, p1, pMan->nBins );
    pAnd->pNext      = pMan->pBins[Key];
    pMan->pBins[Key] = pAnd;
    pMan->nEntries++;
    // create the cuts if defined
//    if ( pAnd->pNtk->pManCut )
//        Abc_NodeGetCuts( pAnd->pNtk->pManCut, pAnd );
    pAnd->oLNode = NULL; 
    pAnd->pCopy = NULL;
    // add the node to the list of updated nodes
    if ( pMan->vAddedCells )
        Vec_PtrPush( pMan->vAddedCells, pAnd );
    return pAnd;
}

/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigAndCreateFrom( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1, Abc_Obj_t * pAnd )
{
    Abc_Obj_t * pTemp;
    unsigned Key;
    assert( !Abc_ObjIsComplement(pAnd) );
    // order the arguments
    if ( Abc_ObjRegular(p0)->Id > Abc_ObjRegular(p1)->Id )
        pTemp = p0, p0 = p1, p1 = pTemp;
    // create the new node
    Abc_ObjAddFanin( pAnd, p0 );
    Abc_ObjAddFanin( pAnd, p1 );
    // set the level of the new node
    pAnd->Level      = 1 + Abc_MaxInt( Abc_ObjRegular(p0)->Level, Abc_ObjRegular(p1)->Level ); 
    pAnd->fExor      = Abc_NodeIsExorType(pAnd);
    // add the node to the corresponding linked list in the table
    Key = Abc_HashKey2( p0, p1, pMan->nBins );
    pAnd->pNext      = pMan->pBins[Key];
    pMan->pBins[Key] = pAnd;
    pMan->nEntries++;
    // create the cuts if defined
//    if ( pAnd->pNtk->pManCut )
//        Abc_NodeGetCuts( pAnd->pNtk->pManCut, pAnd );
    pAnd->pCopy = NULL;
    // add the node to the list of updated nodes
//    if ( pMan->vAddedCells )
//        Vec_PtrPush( pMan->vAddedCells, pAnd );
    return pAnd;
}

/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigAndLookup( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 )
{
    Abc_Obj_t * pAnd, * pConst1;
    unsigned Key;
    assert( Abc_ObjRegular(p0)->pNtk->pManFunc == pMan );
    assert( Abc_ObjRegular(p1)->pNtk->pManFunc == pMan );
    // check for trivial cases
    pConst1 = Abc_AigConst1(pMan->pNtkAig);
    if ( p0 == p1 )
        return p0;
    if ( p0 == Abc_ObjNot(p1) )
        return Abc_ObjNot(pConst1);
    if ( Abc_ObjRegular(p0) == pConst1 )
    {
        if ( p0 == pConst1 )
            return p1;
        return Abc_ObjNot(pConst1);
    }
    if ( Abc_ObjRegular(p1) == pConst1 )
    {
        if ( p1 == pConst1 )
            return p0;
        return Abc_ObjNot(pConst1);
    }
/*
    {
        int nFans0 = Abc_ObjFanoutNum( Abc_ObjRegular(p0) );
        int nFans1 = Abc_ObjFanoutNum( Abc_ObjRegular(p1) );
        if ( nFans0 == 0 || nFans1 == 0 )
            pMan->nStrash0++;
        else if ( nFans0 == 1 || nFans1 == 1 )
            pMan->nStrash1++;
        else if ( nFans0 <= 100 && nFans1 <= 100 )
            pMan->nStrash5++;
        else
            pMan->nStrash2++;
    }
*/
    {
        int nFans0 = Abc_ObjFanoutNum( Abc_ObjRegular(p0) );
        int nFans1 = Abc_ObjFanoutNum( Abc_ObjRegular(p1) );
        if ( nFans0 == 0 || nFans1 == 0 )
            return NULL;
    }

    // order the arguments
    if ( Abc_ObjRegular(p0)->Id > Abc_ObjRegular(p1)->Id )
        pAnd = p0, p0 = p1, p1 = pAnd;
    // get the hash key for these two nodes
    Key = Abc_HashKey2( p0, p1, pMan->nBins );
    // find the matching node in the table
    Abc_AigBinForEachEntry( pMan->pBins[Key], pAnd )
        if ( p0 == Abc_ObjChild0(pAnd) && p1 == Abc_ObjChild1(pAnd) )
        {
//            assert( Abc_ObjFanoutNum(Abc_ObjRegular(p0)) && Abc_ObjFanoutNum(p1) );
             return pAnd;
        }
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Returns the gate implementing EXOR of the two arguments if it exists.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigXorLookup( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1, int * pType )
{
    Abc_Obj_t * pNode1, * pNode2, * pNode;
    // set the flag to zero
    if ( pType ) *pType = 0;
    // check the case of XOR(a,b) = OR(ab, a'b')'
    if ( (pNode1 = Abc_AigAndLookup(pMan, Abc_ObjNot(p0), Abc_ObjNot(p1))) &&
         (pNode2 = Abc_AigAndLookup(pMan, p0, p1)) ) 
    {
        pNode = Abc_AigAndLookup( pMan, Abc_ObjNot(pNode1), Abc_ObjNot(pNode2) );
        if ( pNode && pType ) *pType = 1;
        return pNode;
    }
    // check the case of XOR(a,b) = OR(a'b, ab')
    if ( (pNode1 = Abc_AigAndLookup(pMan, p0, Abc_ObjNot(p1))) &&
         (pNode2 = Abc_AigAndLookup(pMan, Abc_ObjNot(p0), p1)) ) 
    {
        pNode = Abc_AigAndLookup( pMan, Abc_ObjNot(pNode1), Abc_ObjNot(pNode2) );
        return pNode? Abc_ObjNot(pNode) : NULL;
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Returns the gate implementing EXOR of the two arguments if it exists.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigMuxLookup( Abc_Aig_t * pMan, Abc_Obj_t * pC, Abc_Obj_t * pT, Abc_Obj_t * pE, int * pType )
{
    Abc_Obj_t * pNode1, * pNode2, * pNode;
    // set the flag to zero
    if ( pType ) *pType = 0;
    // check the case of MUX(c,t,e) = OR(ct', c'e')'
    if ( (pNode1 = Abc_AigAndLookup(pMan, pC, Abc_ObjNot(pT))) &&
         (pNode2 = Abc_AigAndLookup(pMan, Abc_ObjNot(pC), Abc_ObjNot(pE))) ) 
    {
        pNode = Abc_AigAndLookup( pMan, Abc_ObjNot(pNode1), Abc_ObjNot(pNode2) );
        if ( pNode && pType ) *pType = 1;
        return pNode;
    }
    // check the case of MUX(c,t,e) = OR(ct, c'e)
    if ( (pNode1 = Abc_AigAndLookup(pMan, pC, pT)) &&
         (pNode2 = Abc_AigAndLookup(pMan, Abc_ObjNot(pC), pE)) ) 
    {
        pNode = Abc_AigAndLookup( pMan, Abc_ObjNot(pNode1), Abc_ObjNot(pNode2) );
        return pNode? Abc_ObjNot(pNode) : NULL;
    }
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Deletes an AIG node from the hash table.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigAndDelete( Abc_Aig_t * pMan, Abc_Obj_t * pThis )
{
    Abc_Obj_t * pAnd, * pAnd0, * pAnd1, ** ppPlace;
    unsigned Key;
    assert( !Abc_ObjIsComplement(pThis) );
    assert( Abc_ObjIsNode(pThis) );
    assert( Abc_ObjFaninNum(pThis) == 2 );
    assert( pMan->pNtkAig == pThis->pNtk );
    // get the hash key for these two nodes
    pAnd0 = Abc_ObjRegular( Abc_ObjChild0(pThis) );
    pAnd1 = Abc_ObjRegular( Abc_ObjChild1(pThis) );
    Key = Abc_HashKey2( Abc_ObjChild0(pThis), Abc_ObjChild1(pThis), pMan->nBins );
    // find the matching node in the table
    ppPlace = pMan->pBins + Key;
    Abc_AigBinForEachEntry( pMan->pBins[Key], pAnd )
    {
        if ( pAnd != pThis )
        {
            ppPlace = &pAnd->pNext;
            continue;
        }
        *ppPlace = pAnd->pNext;
        break;
    }
    assert( pAnd == pThis );
    pMan->nEntries--;
    // delete the cuts if defined
    if ( pThis->pNtk->pManCut )
        Abc_NodeFreeCuts( pThis->pNtk->pManCut, pThis );
}

/**Function*************************************************************

  Synopsis    [Resizes the hash table of AIG nodes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigResize( Abc_Aig_t * pMan )
{
    Abc_Obj_t ** pBinsNew;
    Abc_Obj_t * pEnt, * pEnt2;
    int nBinsNew, Counter, i;
    abctime clk;
    unsigned Key;

clk = Abc_Clock();
    // get the new table size
    nBinsNew = Abc_PrimeCudd( 3 * pMan->nBins ); 
    // allocate a new array
    pBinsNew = ABC_ALLOC( Abc_Obj_t *, nBinsNew );
    memset( pBinsNew, 0, sizeof(Abc_Obj_t *) * nBinsNew );
    // rehash the entries from the old table
    Counter = 0;
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntrySafe( pMan->pBins[i], pEnt, pEnt2 )
        {
            Key = Abc_HashKey2( Abc_ObjChild0(pEnt), Abc_ObjChild1(pEnt), nBinsNew );
            pEnt->pNext   = pBinsNew[Key];
            pBinsNew[Key] = pEnt;
            Counter++;
        }
    assert( Counter == pMan->nEntries );
//    printf( "Increasing the structural table size from %6d to %6d. ", pMan->nBins, nBinsNew );
//    ABC_PRT( "Time", Abc_Clock() - clk );
    // replace the table and the parameters
    ABC_FREE( pMan->pBins );
    pMan->pBins = pBinsNew;
    pMan->nBins = nBinsNew;
}

/**Function*************************************************************

  Synopsis    [Resizes the hash table of AIG nodes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigRehash( Abc_Aig_t * pMan )
{
    Abc_Obj_t ** pBinsNew;
    Abc_Obj_t * pEnt, * pEnt2;
    int * pArray;
    unsigned Key;
    int Counter, Temp, i;

    // allocate a new array
    pBinsNew = ABC_ALLOC( Abc_Obj_t *, pMan->nBins );
    memset( pBinsNew, 0, sizeof(Abc_Obj_t *) * pMan->nBins );
    // rehash the entries from the old table
    Counter = 0;
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntrySafe( pMan->pBins[i], pEnt, pEnt2 )
        {
            // swap the fanins if needed
            pArray = pEnt->vFanins.pArray;
            if ( pArray[0] > pArray[1] )
            {
                Temp = pArray[0];
                pArray[0] = pArray[1];
                pArray[1] = Temp;
                Temp = pEnt->fCompl0;
                pEnt->fCompl0 = pEnt->fCompl1;
                pEnt->fCompl1 = Temp;
            }
            // rehash the node
            Key = Abc_HashKey2( Abc_ObjChild0(pEnt), Abc_ObjChild1(pEnt), pMan->nBins );
            pEnt->pNext   = pBinsNew[Key];
            pBinsNew[Key] = pEnt;
            Counter++;
        }
    assert( Counter == pMan->nEntries );
    // replace the table and the parameters
    ABC_FREE( pMan->pBins );
    pMan->pBins = pBinsNew;
}






/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigConst1( Abc_Ntk_t * pNtk )
{
    assert( Abc_NtkIsStrash(pNtk) );
    return ((Abc_Aig_t *)pNtk->pManFunc)->pConst1;
}

/**Function*************************************************************

  Synopsis    [Performs canonicization step.]

  Description [The argument nodes can be complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigAnd( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 )
{
    Abc_Obj_t * pAnd;
    if ( (pAnd = Abc_AigAndLookup( pMan, p0, p1 )) )
        return pAnd;
    return Abc_AigAndCreate( pMan, p0, p1 );
}

/**Function*************************************************************

  Synopsis    [Implements Boolean OR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigOr( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 )
{
    return Abc_ObjNot( Abc_AigAnd( pMan, Abc_ObjNot(p0), Abc_ObjNot(p1) ) );
}

/**Function*************************************************************

  Synopsis    [Implements Boolean XOR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigXor( Abc_Aig_t * pMan, Abc_Obj_t * p0, Abc_Obj_t * p1 )
{
    return Abc_AigOr( pMan, Abc_AigAnd(pMan, p0, Abc_ObjNot(p1)), 
                            Abc_AigAnd(pMan, p1, Abc_ObjNot(p0)) );
}

/**Function*************************************************************

  Synopsis    [Implements Boolean XOR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigMux( Abc_Aig_t * pMan, Abc_Obj_t * pC, Abc_Obj_t * p1, Abc_Obj_t * p0 )
{
    return Abc_AigOr( pMan, Abc_AigAnd(pMan, pC, p1), Abc_AigAnd(pMan, Abc_ObjNot(pC), p0) );
}
 
/**Function*************************************************************

  Synopsis    [Implements the miter.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigMiter_rec( Abc_Aig_t * pMan, Abc_Obj_t ** ppObjs, int nObjs )
{
    Abc_Obj_t * pObj1, * pObj2;
    if ( nObjs == 1 )
        return ppObjs[0];
    pObj1 = Abc_AigMiter_rec( pMan, ppObjs,           nObjs/2 );
    pObj2 = Abc_AigMiter_rec( pMan, ppObjs + nObjs/2, nObjs - nObjs/2 );
    return Abc_AigOr( pMan, pObj1, pObj2 );
}
 
/**Function*************************************************************

  Synopsis    [Implements the miter.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigMiter( Abc_Aig_t * pMan, Vec_Ptr_t * vPairs, int fImplic )
{
    int i;
    if ( vPairs->nSize == 0 )
        return Abc_ObjNot( Abc_AigConst1(pMan->pNtkAig) );
    assert( vPairs->nSize % 2 == 0 );
    // go through the cubes of the node's SOP
    if ( fImplic )
    {
        for ( i = 0; i < vPairs->nSize; i += 2 )
            vPairs->pArray[i/2] = Abc_AigAnd( pMan, (Abc_Obj_t *)vPairs->pArray[i], Abc_ObjNot((Abc_Obj_t *)vPairs->pArray[i+1]) );
    }
    else
    {
        for ( i = 0; i < vPairs->nSize; i += 2 )
            vPairs->pArray[i/2] = Abc_AigXor( pMan, (Abc_Obj_t *)vPairs->pArray[i], (Abc_Obj_t *)vPairs->pArray[i+1] );
    }
    vPairs->nSize = vPairs->nSize/2;
    return Abc_AigMiter_rec( pMan, (Abc_Obj_t **)vPairs->pArray, vPairs->nSize );
}
 
/**Function*************************************************************

  Synopsis    [Implements the miter.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Obj_t * Abc_AigMiter2( Abc_Aig_t * pMan, Vec_Ptr_t * vPairs )
{
    Abc_Obj_t * pMiter, * pXor;
    int i;
    assert( vPairs->nSize % 2 == 0 );
    // go through the cubes of the node's SOP
    pMiter = Abc_ObjNot( Abc_AigConst1(pMan->pNtkAig) );
    for ( i = 0; i < vPairs->nSize; i += 2 )
    {
        pXor   = Abc_AigXor( pMan, (Abc_Obj_t *)vPairs->pArray[i], (Abc_Obj_t *)vPairs->pArray[i+1] );
        pMiter = Abc_AigOr( pMan, pMiter, pXor );
    }
    return pMiter;
}




/**Function*************************************************************

  Synopsis    [Replaces one AIG node by the other.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigReplace( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    assert( Vec_PtrSize(pMan->vStackReplaceOld) == 0 );
    assert( Vec_PtrSize(pMan->vStackReplaceNew) == 0 );
    Vec_PtrPush( pMan->vStackReplaceOld, pOld );
    Vec_PtrPush( pMan->vStackReplaceNew, pNew );
    
    int num_updates = 0;
 
    // printf("##replace old %d new  %d\n", Abc_ObjId(pOld), Abc_ObjId(pNew));
    // int old_level = 0;
    // int old_id = Abc_ObjId(pOld);
    // if (pMan->pNtkAig->vLevelsR)
    //     old_level = Abc_ObjReverseLevel(pOld);
    assert( !Abc_ObjIsComplement(pOld) );
    // process the replacements
    while ( Vec_PtrSize(pMan->vStackReplaceOld) )
    {
        num_updates++;
        pOld = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceOld );
        pNew = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceNew );
        if ( JF_DEBUG_REWRITE )
            printf("##replace old %d new  %d\n", Abc_ObjId(pOld), Abc_ObjId(pNew));
        if ( Abc_ObjFanoutNum(pOld) == 0 )
            //return 0;
            continue;
        Abc_AigReplace_int( pMan, pOld, pNew, fUpdateLevel );
    }
    if (num_updates > 1) printf("##num updates %d node (%d)\n", num_updates, Abc_ObjId(pOld));
    abctime clk = Abc_Clock();
    if ( fUpdateLevel )
    {
        Abc_AigUpdateLevel_int( pMan );
        
        if ( pMan->pNtkAig->vLevelsR ) 
            // Abc_AigUpdateLevelR_new( pMan, pOld );
            Abc_AigUpdateLevelR_int( pMan );
    } 
    
        global_update_time += Abc_Clock() - clk;
    return 1;
}


/**Function*************************************************************

  Synopsis    [Performs internal cleanup step.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigCleanupInc( Abc_Aig_t * pMan )
{
    Vec_Ptr_t * vDangles;
    Abc_Obj_t * pAnd;
    int i, nNodesOld;
//    printf( "Strash0 = %d.  Strash1 = %d.  Strash100 = %d.  StrashM = %d.\n", 
//        pMan->nStrash0, pMan->nStrash1, pMan->nStrash5, pMan->nStrash2 );
    nNodesOld = pMan->nEntries;
    // collect the AND nodes that do not fanout
    vDangles = Vec_PtrAlloc( 100 );
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntry( pMan->pBins[i], pAnd )
            if ( Abc_ObjFanoutNum(pAnd) == 0 )
                Vec_PtrPush( vDangles, pAnd );
    // process the dangling nodes and their MFFCs
    Vec_PtrForEachEntry( Abc_Obj_t *, vDangles, pAnd, i )
        Abc_AigDeleteNodeInc( pMan, pAnd );
    Vec_PtrFree( vDangles );
    return nNodesOld - pMan->nEntries;
}


/**Function*************************************************************

  Synopsis    [Performs internal replacement step.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigReplace_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    Abc_Obj_t * pFanin1, * pFanin2, * pFanout, * pFanoutNew, * pFanoutFanout;
    int k, v, iFanin; 
    // make sure the old node is regular and has fanouts
    // (the new node can be complemented and can have fanouts)
    assert( !Abc_ObjIsComplement(pOld) );
    assert( Abc_ObjFanoutNum(pOld) > 0 );
    // look at the fanouts of old node
    Abc_NodeCollectFanouts( pOld, pMan->vNodes );
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vNodes, pFanout, k )
    {
        if ( Abc_ObjIsCo(pFanout) )
        {
            pFanin1 = Abc_ObjRegular( pNew );
            if ( pFanin1->fMarkB )
                Abc_AigRemoveFromLevelStructureR( pMan->vLevelsR, pFanin1 );
            if ( fUpdateLevel && pMan->pNtkAig->vLevelsR )
            {
                Abc_ObjSetReverseLevel( pFanin1, Abc_ObjReverseLevel(pOld) );
                assert( pFanin1->fMarkB == 0 );
                if ( !Abc_ObjIsCi(pFanin1) )
                {
                    pFanin1->fMarkB = 1;
                    Vec_VecPush( pMan->vLevelsR, Abc_ObjReverseLevel(pFanin1), pFanin1 );
                }
            }
            Abc_ObjPatchFanin( pFanout, pOld, pNew );            
            continue;
        }
        // find the old node as a fanin of this fanout
        iFanin = Vec_IntFind( &pFanout->vFanins, pOld->Id );
        assert( iFanin == 0 || iFanin == 1 );
        // get the new fanin
        pFanin1 = Abc_ObjNotCond( pNew, Abc_ObjFaninC(pFanout, iFanin) );
        assert( Abc_ObjRegular(pFanin1) != pFanout );
        // get another fanin
        pFanin2 = Abc_ObjChild( pFanout, iFanin ^ 1 );
        assert( Abc_ObjRegular(pFanin2) != pFanout );
        // check if the node with these fanins exists
        if ( (pFanoutNew = Abc_AigAndLookup( pMan, pFanin1, pFanin2 )) )
        { // such node exists (it may be a constant)
            // schedule replacement of the old fanout by the new fanout
            // printf("## new fanout has exist\n");
            Vec_PtrPush( pMan->vStackReplaceOld, pFanout );
            Vec_PtrPush( pMan->vStackReplaceNew, pFanoutNew );
            continue;
        }
        // such node does not exist - modify the old fanout node 
        // (this way the change will not propagate all the way to the COs)
        assert( Abc_ObjRegular(pFanin1) != Abc_ObjRegular(pFanin2) );             

        // if the node is in the level structure, remove it
        if ( pFanout->fMarkA ){ 
             Abc_AigRemoveFromLevelStructure( pMan->vLevels, pFanout );
        }
        if (Abc_ObjRegular(pNew)->fMarkB){
           Abc_AigRemoveFromLevelStructureR( pMan->vLevelsR, pFanout ); 
        }

        // remove the old fanout node from the structural hashing table
        Abc_AigAndDelete( pMan, pFanout );
        // remove the fanins of the old fanout
        Abc_ObjRemoveFanins( pFanout );
        // recreate the old fanout with new fanins and add it to the table
        Abc_AigAndCreateFrom( pMan, pFanin1, pFanin2, pFanout );
        assert( Abc_AigNodeIsAcyclic(pFanout, pFanout) );

        if ( fUpdateLevel )
        {
            // schedule the updated fanout for updating direct level
            assert( pFanout->fMarkA == 0 );
            pFanout->fMarkA = 1;
            Vec_VecPush( pMan->vLevels, pFanout->Level, pFanout );
            // schedule the updated fanout for updating reverse level
            if ( pMan->pNtkAig->vLevelsR ) 
            {
               
                // if (Abc_ObjRegular(pNew)->fMarkB == 0){
                //     Abc_ObjRegular(pNew)->fMarkB = 1;
                //     Vec_VecPush( pMan->vLevelsR, Abc_ObjReverseLevel(Abc_ObjRegular(pNew)), Abc_ObjRegular(pNew) );
                // }
                assert( pFanout->fMarkB == 0 );
                pFanout->fMarkB = 1;
                Vec_VecPush( pMan->vLevelsR, Abc_ObjReverseLevel(pFanout), pFanout );
            }
        }
 
                
        // the fanout has changed, update EXOR status of its fanouts
        Abc_ObjForEachFanout( pFanout, pFanoutFanout, v )
            if ( Abc_AigNodeIsAnd(pFanoutFanout) )
                pFanoutFanout->fExor = Abc_NodeIsExorType(pFanoutFanout);
    } 

    // if ( fUpdateLevel )
    //     {
    
    //         // schedule the updated fanout for updating reverse level
    //         if ( pMan->pNtkAig->vLevelsR ) 
    //         {
    //             assert( pFanout->fMarkB == 0 );
    //             pFanout->fMarkB = 1;
    //             Vec_VecPush( pMan->vLevelsR, Abc_ObjReverseLevel(pFanout), pFanout );
    //         }
    //     }


    // if the node has no fanouts left, remove its MFFC
    if ( Abc_ObjFanoutNum(pOld) == 0 )
        Abc_AigDeleteNode( pMan, pOld );
} 

/**Function*************************************************************

  Synopsis    [Performs internal deletion step.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigDeleteNode( Abc_Aig_t * pMan, Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode0, * pNode1, * pTemp;
    int i, k;

    // make sure the node is regular and dangling
    assert( !Abc_ObjIsComplement(pNode) );
    assert( Abc_ObjIsNode(pNode) );
    assert( Abc_ObjFaninNum(pNode) == 2 );
    assert( Abc_ObjFanoutNum(pNode) == 0 );

    // when deleting an old node that is scheduled for replacement, remove it from the replacement queue
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vStackReplaceOld, pTemp, i )
        if ( pNode == pTemp )
        {
            // remove the entry from the replacement array
            for ( k = i; k < pMan->vStackReplaceOld->nSize - 1; k++ )
            {
                pMan->vStackReplaceOld->pArray[k] = pMan->vStackReplaceOld->pArray[k+1];
                pMan->vStackReplaceNew->pArray[k] = pMan->vStackReplaceNew->pArray[k+1];
            }
            pMan->vStackReplaceOld->nSize--;
            pMan->vStackReplaceNew->nSize--;
        }

    // when deleting a new node that should replace another node, do not delete
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vStackReplaceNew, pTemp, i )
        if ( pNode == Abc_ObjRegular(pTemp) )
            return;

    // remember the node's fanins
    pNode0 = Abc_ObjFanin0( pNode );
    pNode1 = Abc_ObjFanin1( pNode );

    // add the node to the list of updated nodes
    if ( pMan->vUpdatedNets )
    {
        Vec_PtrPushUnique( pMan->vUpdatedNets, pNode0 );
        Vec_PtrPushUnique( pMan->vUpdatedNets, pNode1 );
    }

    // remove the node from the table
    Abc_AigAndDelete( pMan, pNode );
    // if the node is in the level structure, remove it
    if ( pNode->fMarkA )
        Abc_AigRemoveFromLevelStructure( pMan->vLevels, pNode );
    if ( pNode->fMarkB )
        Abc_AigRemoveFromLevelStructureR( pMan->vLevelsR, pNode );
    // remove the node from the network
    Abc_NtkDeleteObj( pNode );

    // call recursively for the fanins
    if ( Abc_ObjIsNode(pNode0) && pNode0->vFanouts.nSize == 0 )
        Abc_AigDeleteNode( pMan, pNode0 );
    if ( Abc_ObjIsNode(pNode1) && pNode1->vFanouts.nSize == 0 )
        Abc_AigDeleteNode( pMan, pNode1 );
}


/**Function*************************************************************

  Synopsis    [Updates the level of the node after it has changed.]

  Description [This procedure is based on the observation that
  after the node's level has changed, the fanouts levels can change too, 
  but the new fanout levels are always larger than the node's level.
  As a result, we can accumulate the nodes to be updated in the queue
  and process them in the increasing order of levels.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigUpdateLevel_int( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pNode, * pFanout;
    Vec_Ptr_t * vVec;
    int LevelNew, i, k, v;

    // go through the nodes and update the level of their fanouts
    Vec_VecForEachLevel( pMan->vLevels, vVec, i )
    {
        
        if ( Vec_PtrSize(vVec) == 0 ){
            if ( JF_DEBUG_REWRITE ) printf("##update level %d with empty vec\n", i);
            continue;
        }
        if ( JF_DEBUG_REWRITE ) printf("##update level %d with vec size %d\n", i, Vec_PtrSize(vVec));

        Vec_PtrForEachEntry( Abc_Obj_t *, vVec, pNode, k )
        {
            if ( pNode == NULL )
                continue;
            if ( JF_DEBUG_REWRITE ) printf("\t level %d with node %d\n", i, Abc_ObjId(pNode));
            assert( Abc_ObjIsNode(pNode) );
            assert( (int)pNode->Level == i );
            // clean the mark
            assert( pNode->fMarkA == 1 );
            pNode->fMarkA = 0;
            // iterate through the fanouts
            Abc_ObjForEachFanout( pNode, pFanout, v )
            {
                if ( Abc_ObjIsCo(pFanout) )
                    continue;
                // get the new level of this fanout
                LevelNew = 1 + Abc_MaxInt( Abc_ObjFanin0(pFanout)->Level, Abc_ObjFanin1(pFanout)->Level );
                assert( LevelNew > i );
                if ( (int)pFanout->Level == LevelNew ) // no change
                    continue;
                // if the fanout is present in the data structure, pull it out
                if ( pFanout->fMarkA )
                    Abc_AigRemoveFromLevelStructure( pMan->vLevels, pFanout );
                // update the fanout level
                pFanout->Level = LevelNew;
                // add the fanout to the data structure to update its fanouts
                assert( pFanout->fMarkA == 0 );
                pFanout->fMarkA = 1;
                Vec_VecPush( pMan->vLevels, pFanout->Level, pFanout );
            }
        }
        Vec_PtrClear( vVec );
    }
}

/**Function*************************************************************

  Synopsis    [Updates the level of the node after it has changed.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigUpdateLevelR_int( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pNode, * pFanin, * pFanout;
    Vec_Ptr_t * vVec;
    int LevelNew, i, k, v, j;

    // go through the nodes and update the level of their fanouts
    Vec_VecForEachLevel( pMan->vLevelsR, vVec, i )
    {
        if ( Vec_PtrSize(vVec) == 0 ){
            if ( JF_DEBUG_REWRITE ) printf("##update level r %d with empty vec\n", i);
            continue;
        }
        if ( JF_DEBUG_REWRITE ) printf("##update level r %d with vec size %d\n", i, Vec_PtrSize(vVec));
        Vec_PtrForEachEntry( Abc_Obj_t *, vVec, pNode, k )
        {
            if ( pNode == NULL )
                continue;
            if ( JF_DEBUG_REWRITE ) printf("\t r level %d with node %d\n", i, Abc_ObjId(pNode));
            assert( Abc_ObjIsNode(pNode) );
            assert( Abc_ObjReverseLevel(pNode) == i );
            // clean the mark
            assert( pNode->fMarkB == 1 );
            pNode->fMarkB = 0;
            // iterate through the fanins
            Abc_ObjForEachFanin( pNode, pFanin, v )
            {
                if ( Abc_ObjIsCi(pFanin) )
                    continue;
                // get the new reverse level of this fanin
                LevelNew = 0;
                Abc_ObjForEachFanout( pFanin, pFanout, j )
                    if ( LevelNew < Abc_ObjReverseLevel(pFanout) )
                        LevelNew = Abc_ObjReverseLevel(pFanout);
                LevelNew += 1;
                assert( LevelNew > i );
                if ( Abc_ObjReverseLevel(pFanin) == LevelNew ) // no change
                    continue;
                // if the fanin is present in the data structure, pull it out
                if ( pFanin->fMarkB )
                    Abc_AigRemoveFromLevelStructureR( pMan->vLevelsR, pFanin );
                // update the reverse level
                Abc_ObjSetReverseLevel( pFanin, LevelNew );
                // add the fanin to the data structure to update its fanins
                assert( pFanin->fMarkB == 0 );
                pFanin->fMarkB = 1;
                Vec_VecPush( pMan->vLevelsR, LevelNew, pFanin );
            }
        }
        Vec_PtrClear( vVec );
    }
}




void Abc_AigUpdateLevel_Trigger( Abc_Aig_t * pMan, int candidateLevel, int finalUpdate ){  
    // if current level is larger or equal to the minimum level, perform batch update  
    return; 
    abctime clk = Abc_Clock();
    if (candidateLevel > pMan->nLevelMin)
        Abc_AigUpdateLevelInc_int( pMan );
    // if flag is true, perform final update
    if (finalUpdate)
        Abc_AigUpdateLevelInc_int( pMan );
    // reset the minimum level
    pMan->nLevelMin = ABC_INFINITY;
    global_update_time += Abc_Clock() - clk;

    return; 
}


int Abc_AigUpdateLevel_rec(  Abc_Obj_t * pNode ){  
    
    Abc_Obj_t * pNext;
    int i, Level;
    assert( !Abc_ObjIsNet(pNode) );
    // skip the PI
    if ( Abc_ObjIsCi(pNode) )
        return pNode->Level;
    // using fHandled to prune the nodes that have been rewritten 
    

    assert( Abc_ObjIsNode( pNode ) || pNode->Type == ABC_OBJ_CONST1);
    // if this node is already visited, return
    // if ( Abc_NodeIsTravIdCurrent( pNode ) )
    //     return pNode->Level
    // mark the node as visited
    // Abc_NodeSetTravIdCurrent( pNode ); 
    pNode->Level = 0;
    Abc_ObjForEachFanin( pNode, pNext, i )
    {
        global_level_updates += 1;  
        // Level = Abc_AigUpdateLevel_rec( Abc_ObjFanin0Ntk(pNext) );  
        Level = Abc_AigUpdateLevel_rec( Abc_ObjRegular(pNext) );  
        if ( pNode->Level < (unsigned)Level )
            pNode->Level = Level;
    }   
    if ( Abc_ObjFaninNum(pNode) > 0 && !Abc_ObjIsBarBuf(pNode) )
        pNode->Level++;
    return pNode->Level;    
}
 
void Abc_AigUpdateLevel_Lazy( Abc_Obj_t * pNode ){  
    // perform back dfs until the nodes are visited  
    // may introduce bug here 
    abctime clk = Abc_Clock(); 
    Abc_Obj_t * pFanin0, * pFanin1; 
    pFanin0 = Abc_ObjFanin0(pNode); 
    pFanin1 = Abc_ObjFanin1(pNode); 
    pNode -> Level = Abc_MaxInt(pFanin0->Level, pFanin1->Level) + 1; 
    global_level_updates += 2; 
    global_aig_update_time += Abc_Clock() - clk;
    return;  
    // Abc_AigUpdateLevel_rec( pNode );  
}
  

/**Function*************************************************************

  Synopsis    [Replaces one AIG node by the other, from the view of incremental graph computation ]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigReplaceInc( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    assert( Vec_PtrSize(pMan->vStackReplaceOld) == 0 );
    assert( Vec_PtrSize(pMan->vStackReplaceNew) == 0 );
    Vec_PtrPush( pMan->vStackReplaceOld, pOld );
    Vec_PtrPush( pMan->vStackReplaceNew, pNew );
     
    int num_updates = 0;
     
    if (fUpdateLevel) {
        // clear the priority queue
        P_QueClear(pMan->qLevels);
        P_QueClear(pMan->qLevelsR);
        
        assert(pMan->vTopoAff != NULL);
        Vec_PtrClear(pMan->vTopoAff);
        // // the first node is the order of pTo node, 
        // assert(Abc_ObjRegular(pOld)->oLNode != NULL);
        // Vec_PtrPush(pMan->vTopoAff, Abc_ObjRegular(pOld)->oLNode);
    }
    // process the replacements
    while ( Vec_PtrSize(pMan->vStackReplaceOld) )
    {
        num_updates++;
        pOld = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceOld );
        pNew = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceNew );

       
        if ( Abc_ObjLevel(pOld) < pMan->nLevelMin )
            pMan->nLevelMin = Abc_ObjLevel(pOld);

        if ( JF_DEBUG_REWRITE )
            printf("##replace old %d new  %d\n", Abc_ObjId(pOld), Abc_ObjId(pNew));
        if ( Abc_ObjFanoutNum(pOld) == 0 )
            //return 0;
            continue;
        Abc_AigReplaceInc_int( pMan, pOld, pNew, fUpdateLevel ); 

    }
     
    if ( fUpdateLevel )
    {
        // using lazy updates to maintain the level structure  (batch updates) 
        abctime clk = Abc_Clock();
        // Abc_AigUpdateLevelInc_int( pMan ); 
        if ( pMan->pNtkAig->vLevelsR )  
            Abc_AigUpdateLevelIncR_int( pMan ); 
        global_aig_update_time  += Abc_Clock() - clk;
        
    }   
    return 1;
}

int Abc_AigReplaceIncLevelUpdate( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    assert( Vec_PtrSize(pMan->vStackReplaceOld) == 0 );
    assert( Vec_PtrSize(pMan->vStackReplaceNew) == 0 );
    Vec_PtrPush( pMan->vStackReplaceOld, pOld );
    Vec_PtrPush( pMan->vStackReplaceNew, pNew );
     
    int num_updates = 0;
     
    if (fUpdateLevel) {
        // clear the priority queue
        P_QueClear(pMan->qLevels);
        P_QueClear(pMan->qLevelsR);
        
        assert(pMan->vTopoAff != NULL);
        Vec_PtrClear(pMan->vTopoAff);
        // // the first node is the order of pTo node, 
        // assert(Abc_ObjRegular(pOld)->oLNode != NULL);
        // Vec_PtrPush(pMan->vTopoAff, Abc_ObjRegular(pOld)->oLNode);
    }
    // process the replacements
    while ( Vec_PtrSize(pMan->vStackReplaceOld) )
    {
        num_updates++;
        pOld = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceOld );
        pNew = (Abc_Obj_t *)Vec_PtrPop( pMan->vStackReplaceNew );

       
        if ( Abc_ObjLevel(pOld) < pMan->nLevelMin )
            pMan->nLevelMin = Abc_ObjLevel(pOld);

        if ( JF_DEBUG_REWRITE )
            printf("##replace old %d new  %d\n", Abc_ObjId(pOld), Abc_ObjId(pNew));
        if ( Abc_ObjFanoutNum(pOld) == 0 )
            //return 0;
            continue;
        Abc_AigReplaceIncLevelUpdate_int( pMan, pOld, pNew, fUpdateLevel );  
    }
     
    if ( fUpdateLevel )
    {
        abctime clk = Abc_Clock();
        Abc_AigUpdateLevelInc_int( pMan ); 
        if ( pMan->pNtkAig->vLevelsR )  
            Abc_AigUpdateLevelIncR_int( pMan ); 
        global_aig_update_time  += Abc_Clock() - clk;
        
    }   
    return 1;
}


/**Function*************************************************************

  Synopsis    [Updates the topological order of the nodes in the AIG.]

  Description [Note that: the first node in the affected vector is pOld: with the largest order
  the other nodes in the affect vector are sorted by topological order (DFS)
  ]
               
  SideEffects []

  SeeAlso     []
***********************************************************************/
int Abc_AigReplaceUpdateAff( Abc_Aig_t * pMan ){ 
    abctime clk = Abc_Clock(); 
    Abc_Obj_t * pNode;
    List_Ptr_Node_t * oNodeFirst, * newOrder;
    List_Ptr_t * oList = Abc_AigGetOList(pMan);
    int i; 

    // verify input parameters
    if ( pMan == NULL || oList == NULL )
        return 0;
    if ( pMan->vTopoAff == NULL || Vec_PtrSize(pMan->vTopoAff) == 0 )
        return 1; // nothing to update
    oNodeFirst = pMan->oList->pCurItera;    
    // update the topological order of the nodes in the affected vector 
    Vec_PtrForEachEntry(Abc_Obj_t*, pMan->vTopoAff, pNode, i){ 
        if (pNode == NULL)
            return 0;
          
        // clean the mark
        pNode ->fMarkC = 0;
        if (pNode -> fHandled ) continue; 
  
        // insert the newly created nodes into oList
        // // skip the nodes that are not in the old order
        // if (pNode -> oLNode == NULL)
        //     continue;  

        // create new order for the nodes in AFF
        newOrder = List_PtrInsertAfter(oList, oNodeFirst, pNode); 
        assert(newOrder != NULL);
        // remove the node from the old order
        List_PtrRemoveNode(oList, pNode->oLNode);
        // update the node's order
        pNode->oLNode = newOrder;
        oNodeFirst = newOrder; 
        global_reorder_nodes++;
    } 
    global_aig_update_time += Abc_Clock() - clk; 
    // free the nodes in vAffTmp
    Vec_PtrErase(pMan->vTopoAff);
    return 1; 
}

   



/**Function*************************************************************

  Synopsis    [Find the affected nodes based on the created edge]

  Description [The nodes are stored in the vector vTopoAff]
               
  SideEffects []    

  SeeAlso     []

***********************************************************************/    
void Abc_AigReplaceFindAff( Abc_Aig_t * pMan, Abc_Obj_t * pFrom ){ 
    assert(pFrom != NULL);
    if (Abc_ObjIsCi(pFrom) || Abc_AigNodeIsConst(pFrom)) 
        return; 
    if (pFrom -> fHandled)
        return; 
    if (pFrom == (Abc_Obj_t *) pMan ->oList->pCurItera->pData )
        return; 
 
    Abc_AigReplaceFindAff_rec( pMan, pFrom); 
    // the last node is the pFrom node, if it already has in the AIG table     
    Vec_PtrPush(pMan->vTopoAff, pFrom);
    pFrom->fMarkC = 1; 
    return; 
}

void  Abc_AigReplaceFindAff_rec( Abc_Aig_t * pMan, Abc_Obj_t * pFrom ){ 
    Abc_Obj_t * pFanin; 
    int i; 
    Abc_ObjForEachFanin( pFrom, pFanin, i ){
        if (Abc_ObjIsCi(pFanin) || Abc_AigNodeIsConst(pFanin) )
                continue; 
        if (pFanin -> fHandled)
            continue;   
        if (pFanin -> fMarkC)
            continue;
        // skip the current iterator node 
        if (pFanin == (Abc_Obj_t *) pMan ->oList->pCurItera->pData )
            continue; 
        pFanin->fMarkC = 1; 
        Abc_AigReplaceFindAff_rec( pMan, pFanin ); 
        // push the node to the topoAff
        // note that the nodes without oLNode may also be pushed to the topoAff
        Vec_PtrPush(pMan->vTopoAff, pFanin);  
    }
    return; 
}   


/**Function*************************************************************

  Synopsis    [Performs internal replacement step, with priority queue to update the level]

  Description [ 1. fMarkA is used to mark the node to be updated in the level structure, 
                2. fMarkB is used to mark the node to be updated in the reverse level structure]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigReplaceInc_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    Abc_Obj_t * pFanin1, * pFanin2, * pFanout, * pFanoutNew, * pFanoutFanout;
    int k, v, iFanin;
    // int minFanoutLevel = ABC_INFINITY; 
    int isFirst = 1; 

    // make sure the old node is regular and has fanouts
    // (the new node can be complemented and can have fanouts)
    assert( !Abc_ObjIsComplement(pOld) );
    assert( Abc_ObjFanoutNum(pOld) > 0 );
    // look at the fanouts of old node
    Abc_NodeCollectFanouts( pOld, pMan->vNodes );
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vNodes, pFanout, k )
    {
        if ( Abc_ObjIsCo(pFanout) )
        {
            
            pFanin1 = Abc_ObjRegular( pNew );
            if ( pFanin1->fMarkB )
                pFanin1->fMarkB = 0; 
            // Lemma . 
            if ( fUpdateLevel && pMan->pNtkAig->vLevelsR )
            {
                Abc_ObjSetReverseLevel( pFanin1, Abc_ObjReverseLevel(pOld) );
                assert( pFanin1->fMarkB == 0 );
                if ( !Abc_ObjIsCi(pFanin1) )
                {
                    pFanin1->fMarkB = 1; 
                    P_QuePush(pMan->qLevelsR, pFanin1, 1.0 * Abc_ObjReverseLevel(pFanin1)); 
                }
            }
            Abc_ObjPatchFanin( pFanout, pOld, pNew );            
            continue;  
        }
        // find the old node as a fanin of this fanout
        iFanin = Vec_IntFind( &pFanout->vFanins, pOld->Id );
        assert( iFanin == 0 || iFanin == 1 );
        // get the new fanin
        pFanin1 = Abc_ObjNotCond( pNew, Abc_ObjFaninC(pFanout, iFanin) );
        assert( Abc_ObjRegular(pFanin1) != pFanout );
        // get another fanin
        pFanin2 = Abc_ObjChild( pFanout, iFanin ^ 1 );
        assert( Abc_ObjRegular(pFanin2) != pFanout );
        // check if the node with these fanins exists
        if ( (pFanoutNew = Abc_AigAndLookup( pMan, pFanin1, pFanin2 )) )
        { // such node exists (it may be a constant)
            // schedule replacement of the old fanout by the new fanout  
            Vec_PtrPush( pMan->vStackReplaceOld, pFanout );
            Vec_PtrPush( pMan->vStackReplaceNew, pFanoutNew );
            continue;
        }
        // such node does not exist - modify the old fanout node 
        // (this way the change will not propagate all the way to the COs)
        assert( Abc_ObjRegular(pFanin1) != Abc_ObjRegular(pFanin2) );             
        
        if (isFirst && fUpdateLevel) {
            abctime clk = Abc_Clock();
            Abc_AigReplaceFindAff( pMan, Abc_ObjRegular(pNew)); 
            global_aig_update_time += Abc_Clock() - clk;
            // // print the affected nodes
            // Abc_Obj_t * pNodeTmp;
            // int i = 0; 
            // printf("\t topoAff: ");
            // Vec_PtrForEachEntry(Abc_Obj_t*, pMan->vTopoAff, pNodeTmp, i){
            //     printf("%d ", Abc_ObjId(pNodeTmp));
            // }
            // printf("\n"); 
            isFirst = 0;
        }
 
        // remove the old fanout node from the structural hashing table
        Abc_AigAndDelete( pMan, pFanout );
        // remove the fanins of the old fanout
        Abc_ObjRemoveFanins( pFanout );
        // recreate the old fanout with new fanins and add it to the table
        Abc_AigAndCreateFrom( pMan, pFanin1, pFanin2, pFanout );

 
        assert( Abc_AigNodeIsAcyclic(pFanout, pFanout) );

        if ( fUpdateLevel )
        {
            // if (pFanout->fMarkA == 0){
            //     pFanout->fMarkA = 1;
            //     P_QuePush(pMan->qLevels, pFanout, 1.0 * pFanout->Level); 
            // }   

            if (pMan->pNtkAig->vLevelsR){
                if (pFanout->fMarkB == 0){   // && pFanout->fUpdated == 0
                    pFanout->fMarkB = 1;
                    P_QuePush(pMan->qLevelsR, pFanout, 1.0 * Abc_ObjReverseLevel(pFanout)); 
                }
            } 
        }

        // the fanout has changed, update EXOR status of its fanouts
        Abc_ObjForEachFanout( pFanout, pFanoutFanout, v )
            if ( Abc_AigNodeIsAnd(pFanoutFanout) )
                pFanoutFanout->fExor = Abc_NodeIsExorType(pFanoutFanout);
    }  
    // if the node has no fanouts left, remove its MFFC
    if ( Abc_ObjFanoutNum(pOld) == 0 )
        Abc_AigDeleteNodeInc( pMan, pOld );     
    // clear the candidate for reverse level maintanance
    if (!fUpdateLevel) { 
        Abc_Obj_t * pNodeTmp;
        while (P_QueSize(pMan->qLevelsR) > 0){
            pNodeTmp = (Abc_Obj_t *)P_QuePop(pMan->qLevelsR);
            pNodeTmp->fMarkB = 0;
        }
    }
} 

void Abc_AigReplaceIncLevelUpdate_int( Abc_Aig_t * pMan, Abc_Obj_t * pOld, Abc_Obj_t * pNew, int fUpdateLevel )
{
    Abc_Obj_t * pFanin1, * pFanin2, * pFanout, * pFanoutNew, * pFanoutFanout;
    int k, v, iFanin;
    // int minFanoutLevel = ABC_INFINITY; 
    int isFirst = 1; 

    // make sure the old node is regular and has fanouts
    // (the new node can be complemented and can have fanouts)
    assert( !Abc_ObjIsComplement(pOld) );
    assert( Abc_ObjFanoutNum(pOld) > 0 );
    // look at the fanouts of old node
    Abc_NodeCollectFanouts( pOld, pMan->vNodes );
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vNodes, pFanout, k )
    {
        if ( Abc_ObjIsCo(pFanout) )
        {
            
            pFanin1 = Abc_ObjRegular( pNew );
            if ( pFanin1->fMarkB )
                pFanin1->fMarkB = 0; 
            // Lemma . 
            if ( fUpdateLevel && pMan->pNtkAig->vLevelsR )
            {
                Abc_ObjSetReverseLevel( pFanin1, Abc_ObjReverseLevel(pOld) );
                assert( pFanin1->fMarkB == 0 );
                if ( !Abc_ObjIsCi(pFanin1) )
                {
                    pFanin1->fMarkB = 1; 
                    P_QuePush(pMan->qLevelsR, pFanin1, 1.0 * Abc_ObjReverseLevel(pFanin1)); 
                }
            }
            Abc_ObjPatchFanin( pFanout, pOld, pNew );            
            continue;  
        }
        // find the old node as a fanin of this fanout
        iFanin = Vec_IntFind( &pFanout->vFanins, pOld->Id );
        assert( iFanin == 0 || iFanin == 1 );
        // get the new fanin
        pFanin1 = Abc_ObjNotCond( pNew, Abc_ObjFaninC(pFanout, iFanin) );
        assert( Abc_ObjRegular(pFanin1) != pFanout );
        // get another fanin
        pFanin2 = Abc_ObjChild( pFanout, iFanin ^ 1 );
        assert( Abc_ObjRegular(pFanin2) != pFanout );
        // check if the node with these fanins exists
        if ( (pFanoutNew = Abc_AigAndLookup( pMan, pFanin1, pFanin2 )) )
        { // such node exists (it may be a constant)
            // schedule replacement of the old fanout by the new fanout  
            Vec_PtrPush( pMan->vStackReplaceOld, pFanout );
            Vec_PtrPush( pMan->vStackReplaceNew, pFanoutNew );
            continue;
        }
        // such node does not exist - modify the old fanout node 
        // (this way the change will not propagate all the way to the COs)
        assert( Abc_ObjRegular(pFanin1) != Abc_ObjRegular(pFanin2) );             
        
        if (isFirst && fUpdateLevel) {
            abctime clk = Abc_Clock();
            Abc_AigReplaceFindAff( pMan, Abc_ObjRegular(pNew)); 
            global_aig_update_time += Abc_Clock() - clk;
            // // print the affected nodes
            // Abc_Obj_t * pNodeTmp;
            // int i = 0; 
            // printf("\t topoAff: ");
            // Vec_PtrForEachEntry(Abc_Obj_t*, pMan->vTopoAff, pNodeTmp, i){
            //     printf("%d ", Abc_ObjId(pNodeTmp));
            // }
            // printf("\n"); 
            isFirst = 0;
        }
 
        // remove the old fanout node from the structural hashing table
        Abc_AigAndDelete( pMan, pFanout );
        // remove the fanins of the old fanout
        Abc_ObjRemoveFanins( pFanout );
        // recreate the old fanout with new fanins and add it to the table
        Abc_AigAndCreateFrom( pMan, pFanin1, pFanin2, pFanout );

 
        assert( Abc_AigNodeIsAcyclic(pFanout, pFanout) );

        if ( fUpdateLevel )
        {
            if (pFanout->fMarkA == 0){
                pFanout->fMarkA = 1;
                P_QuePush(pMan->qLevels, pFanout, 1.0 * pFanout->Level); 
            }   

            if (pMan->pNtkAig->vLevelsR){
                if (pFanout->fMarkB == 0){   // && pFanout->fUpdated == 0
                    pFanout->fMarkB = 1;
                    P_QuePush(pMan->qLevelsR, pFanout, 1.0 * Abc_ObjReverseLevel(pFanout)); 
                }
            } 
        }

        // the fanout has changed, update EXOR status of its fanouts
        Abc_ObjForEachFanout( pFanout, pFanoutFanout, v )
            if ( Abc_AigNodeIsAnd(pFanoutFanout) )
                pFanoutFanout->fExor = Abc_NodeIsExorType(pFanoutFanout);
    }  
    // if the node has no fanouts left, remove its MFFC
    if ( Abc_ObjFanoutNum(pOld) == 0 )
        Abc_AigDeleteNodeInc( pMan, pOld );     
    // clear the candidate for reverse level maintanance
    if (!fUpdateLevel) { 
        Abc_Obj_t * pNodeTmp;
        while (P_QueSize(pMan->qLevelsR) > 0){
            pNodeTmp = (Abc_Obj_t *)P_QuePop(pMan->qLevelsR);
            pNodeTmp->fMarkB = 0;
        }
    }
}


/**Function*************************************************************

  Synopsis    [New algorithm for updating the level of the node after it has changed.]

  Description [using priority queue to update the level]
               
  SideEffects []

***********************************************************************/
void Abc_AigUpdateLevelInc_int( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pNode, * pFanout;
    int LevelNew, v;

    // using priority queue to update the level
    while (P_QueSize(pMan->qLevels) > 0){
        pNode = P_QuePop(pMan->qLevels); 
        global_level_updates++;
        // we do not remove the pNode from pMan->vQueue in Abc_AigDeleteNodeInc
        // so we need first to check pNode->fMarkA rather than assert pNode == NULL
        if (pNode->fMarkA == 0) continue;
        // reset pNode umark, as it has removed from the queue
        pNode->fMarkA = 0;

        if (pNode == NULL) continue;
        assert( Abc_ObjIsNode(pNode) );
         
        // iterate through the fanouts  
        Abc_ObjForEachFanout( pNode, pFanout, v )
        {
            if ( Abc_ObjIsCo(pFanout) )
                continue;
            
            // if (pNode ->oLNode != NULL)
            //     continue; 

            // get the new level of this fanout
            LevelNew = 1 + Abc_MaxInt( Abc_ObjFanin0(pFanout)->Level, Abc_ObjFanin1(pFanout)->Level );
            if ( (int)pFanout->Level == LevelNew ) // no change
                continue;
            
            // update the fanout level
            pFanout->Level = LevelNew;   
            // push the fanout to the queue
            // if (pFanout->fMarkA == 0 && (pFanout->oLNode == NULL || pFanout->fHandled == 1)){
            if (pFanout->fMarkA == 0){
                pFanout->fMarkA = 1;
                P_QuePush(pMan->qLevels, pFanout, 1.0 * pFanout->Level); 
            }  
        }
    }
 
}
 

void Abc_AigUpdateLevelIncR_int( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pNode, * pFanin, * pFanout;
    int LevelNew, j, v;

    // using priority queue to update the level
    while (P_QueSize(pMan->qLevelsR) > 0){
        pNode = (Abc_Obj_t *)P_QuePop(pMan->qLevelsR);
        global_reverse_updates++;
        if (pNode->fMarkB == 0) continue;
        pNode->fMarkB = 0;
        if (pNode == NULL) continue;
        assert( Abc_ObjIsNode(pNode) );
        // iterate through the fanins
        Abc_ObjForEachFanin( pNode, pFanin, v )
        {
            if ( Abc_ObjIsCi(pFanin) || Abc_AigNodeIsConst(pFanin) )
                continue;

            if (pFanin->fHandled) // 
                continue;

            // get the new reverse level of this fanin
            LevelNew = 0;
                        
            Abc_ObjForEachFanout( pFanin, pFanout, j )
                if ( LevelNew < Abc_ObjReverseLevel(pFanout) )
                    LevelNew = Abc_ObjReverseLevel(pFanout);
            LevelNew += 1;
           
            if (  Abc_ObjReverseLevel(pFanin) == LevelNew  ) // no change
                continue;
            // update the reverse level
            Abc_ObjSetReverseLevel( pFanin, LevelNew );
            // push the fanin to the queue
            if (pFanin->fMarkB == 0){
                pFanin->fMarkB = 1;
                P_QuePush(pMan->qLevelsR, pFanin, 1.0 * pFanin->Level); 
            }   
        }
    } 
}

   

void Abc_AigDeleteNodeInc( Abc_Aig_t * pMan, Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNode0, * pNode1, * pTemp;
    int i, k;

    // make sure the node is regular and dangling
    assert( !Abc_ObjIsComplement(pNode) );
    assert( Abc_ObjIsNode(pNode) );
    assert( Abc_ObjFaninNum(pNode) == 2 );
    assert( Abc_ObjFanoutNum(pNode) == 0 );

    // when deleting an old node that is scheduled for replacement, remove it from the replacement queue
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vStackReplaceOld, pTemp, i )
        if ( pNode == pTemp )
        {
            // remove the entry from the replacement array
            for ( k = i; k < pMan->vStackReplaceOld->nSize - 1; k++ )
            {
                pMan->vStackReplaceOld->pArray[k] = pMan->vStackReplaceOld->pArray[k+1];
                pMan->vStackReplaceNew->pArray[k] = pMan->vStackReplaceNew->pArray[k+1];
            }
            pMan->vStackReplaceOld->nSize--;
            pMan->vStackReplaceNew->nSize--;
        }

    // when deleting a new node that should replace another node, do not delete
    Vec_PtrForEachEntry( Abc_Obj_t *, pMan->vStackReplaceNew, pTemp, i )
        if ( pNode == Abc_ObjRegular(pTemp) )
            return;

    // remember the node's fanins
    pNode0 = Abc_ObjFanin0( pNode );
    pNode1 = Abc_ObjFanin1( pNode );

    // add the node to the list of updated nodes
    if ( pMan->vUpdatedNets )
    {
        Vec_PtrPushUnique( pMan->vUpdatedNets, pNode0 );
        Vec_PtrPushUnique( pMan->vUpdatedNets, pNode1 );
    }
     
    // duplicate the list node, with null pData. 
    // pMan ->oList != NULL   :  when delete the node in fUpdateLevel mode
    // pNode ->oLNode != NULL :  to skip the new created node 
    // we should redirect the current oLNode iterator
    if (pMan ->oList != NULL && pNode ->oLNode != NULL) {
        List_Ptr_Node_t * tmp = List_PtrDupNodeEmpty (pMan->oList, pNode->oLNode);
        // current iteraotr is deleted. 
        if (pNode ->oLNode == pMan->oList->pCurItera) {
            pMan->oList->pCurItera = tmp; 
        } 
    }        
 
    
    // then remove the node from the table
    Abc_AigAndDelete( pMan, pNode );
    // if the node is in the level structure, remove it
    // Note: different from Abc_AigUpdateLevel_int, we only mark the node as unmarked when the node is deleted  
    //       because we do not remove the node from pMan->vQueue in Abc_AigDeleteNodeInc
    if ( pNode->fMarkA )
        pNode->fMarkA = 0; 
    if ( pNode->fMarkB )
        pNode->fMarkB = 0;
        
    // remove the node from the network
    Abc_NtkDeleteObj( pNode );

    
    // call recursively for the fanins
    if ( Abc_ObjIsNode(pNode0) && pNode0->vFanouts.nSize == 0 ) {
        Abc_AigDeleteNodeInc( pMan, pNode0 );
    } else {
        // if the node is not handled and not in the queue, push it to the queue, reverse level maintanance
        if (pNode0->fHandled == 0 && pNode0->fMarkB == 0 && Abc_ObjIsNode(pNode0)) {
            pNode0->fMarkB = 1;
            P_QuePush(pMan->qLevelsR, pNode0, 1.0 * Abc_ObjReverseLevel(pNode0));
        }
    }
        

    if ( Abc_ObjIsNode(pNode1) && pNode1->vFanouts.nSize == 0 ) {
        Abc_AigDeleteNodeInc( pMan, pNode1 );
    } else {
        // if the node is not handled and not in the queue, push it to the queue, reverse level maintanance
        if (pNode1->fHandled == 0 && pNode1->fMarkB == 0 && Abc_ObjIsNode(pNode1)) {
            pNode1->fMarkB = 1;
            P_QuePush(pMan->qLevelsR, pNode1, 1.0 * Abc_ObjReverseLevel(pNode1));
        }
    }

    
    // if (pNode0->fHandled == 0 && pNode0->fMarkB == 0) {
    //         pNode0->fMarkB = 1;
    //         P_QuePush(pMan->qLevelsR, pNode0, 1.0 * Abc_ObjReverseLevel(pNode0));
    // }
}


/**Function*************************************************************

  Synopsis    [New algorithm for updating the level of the node after it has changed.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigUpdateLevelR_new( Abc_Aig_t * pMan, Abc_Obj_t * pOld )
{
    Abc_Obj_t * pNode, * pFanin, * pFanout;
    Vec_Ptr_t * vVec;
    int LevelNew, i, k, v, j;

    // go through the nodes and update the level of their fanouts
    Vec_VecForEachLevel( pMan->vLevelsR, vVec, i )
    {
        if ( Vec_PtrSize(vVec) == 0 ){
            if ( JF_DEBUG_REWRITE ) printf("##update level r %d with empty vec\n", i);
            continue;
        }
        if ( JF_DEBUG_REWRITE ) printf("##update level r %d with vec size %d\n", i, Vec_PtrSize(vVec));
        Vec_PtrForEachEntry( Abc_Obj_t *, vVec, pNode, k )
        {
            if ( pNode == NULL )
                continue;
            if ( JF_DEBUG_REWRITE ) printf("\t r level %d with node %d\n", i, Abc_ObjId(pNode));
            assert( Abc_ObjIsNode(pNode) );
            assert( Abc_ObjReverseLevel(pNode) == i );
            // clean the mark
            assert( pNode->fMarkB == 1 );
            pNode->fMarkB = 0;
            // iterate through the fanins
            Abc_ObjForEachFanin( pNode, pFanin, v )
            {
                if ( Abc_ObjIsCi(pFanin) )
                    continue;
                // get the new reverse level of this fanin
                LevelNew = 0;
                Abc_ObjForEachFanout( pFanin, pFanout, j )
                    if ( LevelNew < Abc_ObjReverseLevel(pFanout) )
                        LevelNew = Abc_ObjReverseLevel(pFanout);
                LevelNew += 1;
                assert( LevelNew > i );
                if ( Abc_ObjReverseLevel(pFanin) == LevelNew ) // no change
                    continue;
                // if the fanin is present in the data structure, pull it out
                if ( pFanin->fMarkB )
                    Abc_AigRemoveFromLevelStructureR( pMan->vLevelsR, pFanin );
                // update the reverse level
                Abc_ObjSetReverseLevel( pFanin, LevelNew );
                // add the fanin to the data structure to update its fanins
                assert( pFanin->fMarkB == 0 );
                pFanin->fMarkB = 1;
                Vec_VecPush( pMan->vLevelsR, LevelNew, pFanin );
            }
        }
        Vec_PtrClear( vVec );
    }
}

/**Function*************************************************************

  Synopsis    [Removes the node from the level structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigRemoveFromLevelStructure( Vec_Vec_t * vStruct, Abc_Obj_t * pNode )
{
    Vec_Ptr_t * vVecTemp;
    Abc_Obj_t * pTemp;
    int m;
    assert( pNode->fMarkA );
    vVecTemp = Vec_VecEntry( vStruct, pNode->Level );
    Vec_PtrForEachEntry( Abc_Obj_t *, vVecTemp, pTemp, m )
    {
        if ( pTemp != pNode )
            continue;
        Vec_PtrWriteEntry( vVecTemp, m, NULL );
        break;
    }
    assert( m < Vec_PtrSize(vVecTemp) ); // found
    pNode->fMarkA = 0;
}

/**Function*************************************************************

  Synopsis    [Removes the node from the level structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigRemoveFromLevelStructureR( Vec_Vec_t * vStruct, Abc_Obj_t * pNode )
{
    Vec_Ptr_t * vVecTemp;
    Abc_Obj_t * pTemp;
    int m;
    assert( pNode->fMarkB );
    vVecTemp = Vec_VecEntry( vStruct, Abc_ObjReverseLevel(pNode) );
    Vec_PtrForEachEntry( Abc_Obj_t *, vVecTemp, pTemp, m )
    {
        if ( pTemp != pNode )
            continue;
        Vec_PtrWriteEntry( vVecTemp, m, NULL );
        break;
    }
    assert( m < Vec_PtrSize(vVecTemp) ); // found
    pNode->fMarkB = 0;
}




/**Function*************************************************************

  Synopsis    [Returns 1 if the node has at least one complemented fanout.]

  Description [A fanout is complemented if the fanout's fanin edge pointing
  to the given node is complemented.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigNodeHasComplFanoutEdge( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pFanout;
    int i, iFanin;
    Abc_ObjForEachFanout( pNode, pFanout, i )
    {
        iFanin = Vec_IntFind( &pFanout->vFanins, pNode->Id );
        assert( iFanin >= 0 );
        if ( Abc_ObjFaninC( pFanout, iFanin ) )
            return 1;
    }
    return 0;
}

/**Function*************************************************************

  Synopsis    [Returns 1 if the node has at least one complemented fanout.]

  Description [A fanout is complemented if the fanout's fanin edge pointing
  to the given node is complemented. Only the fanouts with current TravId
  are counted.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigNodeHasComplFanoutEdgeTrav( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pFanout;
    int i, iFanin;
    Abc_ObjForEachFanout( pNode, pFanout, i )
    {
        if ( !Abc_NodeIsTravIdCurrent(pFanout) )
            continue;
        iFanin = Vec_IntFind( &pFanout->vFanins, pNode->Id );
        assert( iFanin >= 0 );
        if ( Abc_ObjFaninC( pFanout, iFanin ) )
            return 1;
    }
    return 0;
}


/**Function*************************************************************

  Synopsis    [Prints the AIG node for debugging purposes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigPrintNode( Abc_Obj_t * pNode )
{
    Abc_Obj_t * pNodeR = Abc_ObjRegular(pNode);
    if ( Abc_ObjIsCi(pNodeR) )
    {
        printf( "CI %4s%s.\n", Abc_ObjName(pNodeR), Abc_ObjIsComplement(pNode)? "\'" : "" );
        return;
    }
    if ( Abc_AigNodeIsConst(pNodeR) )
    {
        printf( "Constant 1 %s.\n", Abc_ObjIsComplement(pNode)? "(complemented)" : ""  );
        return;
    }
    // print the node's function
    printf( "%7s%s", Abc_ObjName(pNodeR),                Abc_ObjIsComplement(pNode)? "\'" : "" );
    printf( " = " );
    printf( "%7s%s", Abc_ObjName(Abc_ObjFanin0(pNodeR)), Abc_ObjFaninC0(pNodeR)?     "\'" : "" );
    printf( " * " );
    printf( "%7s%s", Abc_ObjName(Abc_ObjFanin1(pNodeR)), Abc_ObjFaninC1(pNodeR)?     "\'" : "" );
    printf( "\n" );
}


/**Function*************************************************************

  Synopsis    [Check if the node has a combination loop of depth 1 or 2.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigNodeIsAcyclic( Abc_Obj_t * pNode, Abc_Obj_t * pRoot )
{
    Abc_Obj_t * pFanin0, * pFanin1;
    Abc_Obj_t * pChild00, * pChild01;
    Abc_Obj_t * pChild10, * pChild11;
    if ( !Abc_AigNodeIsAnd(pNode) )
        return 1;
    pFanin0 = Abc_ObjFanin0(pNode);
    pFanin1 = Abc_ObjFanin1(pNode);
    if ( pRoot == pFanin0 || pRoot == pFanin1 )
        return 0;
    if ( Abc_ObjIsCi(pFanin0) )
    {
        pChild00 = NULL;
        pChild01 = NULL;
    }
    else
    {
        pChild00 = Abc_ObjFanin0(pFanin0);
        pChild01 = Abc_ObjFanin1(pFanin0);
        if ( pRoot == pChild00 || pRoot == pChild01 )
            return 0;
    }
    if ( Abc_ObjIsCi(pFanin1) )
    {
        pChild10 = NULL;
        pChild11 = NULL;
    }
    else
    {
        pChild10 = Abc_ObjFanin0(pFanin1);
        pChild11 = Abc_ObjFanin1(pFanin1);
        if ( pRoot == pChild10 || pRoot == pChild11 )
            return 0;
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Resizes the hash table of AIG nodes.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigCheckFaninOrder( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pEnt;
    int i;
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntry( pMan->pBins[i], pEnt )
        {
            if ( Abc_ObjRegular(Abc_ObjChild0(pEnt))->Id > Abc_ObjRegular(Abc_ObjChild1(pEnt))->Id )
            {
//                int i0 = Abc_ObjRegular(Abc_ObjChild0(pEnt))->Id;
//                int i1 = Abc_ObjRegular(Abc_ObjChild1(pEnt))->Id;
                printf( "Node %d has incorrect ordering of fanins.\n", pEnt->Id );
            }
        }
}

/**Function*************************************************************

  Synopsis    [Sets the correct phase of the nodes.]

  Description [The AIG nodes should be in the DFS order.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigSetNodePhases( Abc_Ntk_t * pNtk )
{
    Abc_Obj_t * pObj;
    int i;
    assert( Abc_NtkIsDfsOrdered(pNtk) );
    Abc_AigConst1(pNtk)->fPhase = 1;
    Abc_NtkForEachPi( pNtk, pObj, i )
        pObj->fPhase = 0;
    Abc_NtkForEachLatchOutput( pNtk, pObj, i )
        pObj->fPhase = Abc_LatchIsInit1(pObj);
    Abc_AigForEachAnd( pNtk, pObj, i )
        pObj->fPhase = (Abc_ObjFanin0(pObj)->fPhase ^ Abc_ObjFaninC0(pObj)) & (Abc_ObjFanin1(pObj)->fPhase ^ Abc_ObjFaninC1(pObj));
    Abc_NtkForEachPo( pNtk, pObj, i )
        pObj->fPhase = (Abc_ObjFanin0(pObj)->fPhase ^ Abc_ObjFaninC0(pObj));
    Abc_NtkForEachLatchInput( pNtk, pObj, i )
        pObj->fPhase = (Abc_ObjFanin0(pObj)->fPhase ^ Abc_ObjFaninC0(pObj));
}



/**Function*************************************************************

  Synopsis    [Start the update list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Abc_AigUpdateStart( Abc_Aig_t * pMan, Vec_Ptr_t ** pvUpdatedNets )
{
    assert( pMan->vAddedCells == NULL );
    pMan->vAddedCells  = Vec_PtrAlloc( 1000 );
    pMan->vUpdatedNets = Vec_PtrAlloc( 1000 );
    *pvUpdatedNets = pMan->vUpdatedNets;
    return pMan->vAddedCells;
}

/**Function*************************************************************

  Synopsis    [Start the update list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigUpdateStop( Abc_Aig_t * pMan )
{
    assert( pMan->vAddedCells != NULL );
    Vec_PtrFree( pMan->vAddedCells );
    Vec_PtrFree( pMan->vUpdatedNets );
    pMan->vAddedCells = NULL;
    pMan->vUpdatedNets = NULL;
}

/**Function*************************************************************

  Synopsis    [Start the update list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_AigUpdateReset( Abc_Aig_t * pMan )
{
    assert( pMan->vAddedCells != NULL );
    Vec_PtrClear( pMan->vAddedCells );
    Vec_PtrClear( pMan->vUpdatedNets );
}

/**Function*************************************************************

  Synopsis    [Start the update list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_AigCountNext( Abc_Aig_t * pMan )
{
    Abc_Obj_t * pAnd;
    int i, Counter = 0, CounterTotal = 0;
    // count how many nodes have pNext set
    for ( i = 0; i < pMan->nBins; i++ )
        Abc_AigBinForEachEntry( pMan->pBins[i], pAnd )
        {
            Counter += (pAnd->pNext != NULL);
            CounterTotal++;
        }
    printf( "Counter = %d.  Nodes = %d.  Ave = %6.2f\n", Counter, CounterTotal, 1.0 * CounterTotal/pMan->nBins );
    return Counter;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


void Abc_NtkHelloWorld( Abc_Ntk_t * pNtk )
{
    printf( "Hello, World!\n" );
}


ABC_NAMESPACE_IMPL_END

