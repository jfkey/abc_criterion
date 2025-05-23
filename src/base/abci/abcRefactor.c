/**CFile****************************************************************

  FileName    [abcRefactor.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Resynthesis based on collapsing and refactoring.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcRefactor.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "bool/dec/dec.h"
#include "bool/kit/kit.h"

ABC_NAMESPACE_IMPL_START

extern abctime global_time; 
extern abctime global_resynthesis_time; 
extern abctime global_cut_time; 
extern abctime global_aig_update_time; 
extern abctime global_aig_converter_time;  
extern long int global_level_updates;
extern long int global_reverse_updates;
extern long int global_node_rewritten; 
extern long int global_reorder_nodes;

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Abc_ManRef_t_   Abc_ManRef_t;
struct Abc_ManRef_t_
{
    // user specified parameters
    int              nNodeSizeMax;      // the limit on the size of the supernode
    int              nConeSizeMax;      // the limit on the size of the containing cone
    int              fVerbose;          // the verbosity flag
    // internal data structures
    Vec_Ptr_t *      vVars;             // truth tables
    Vec_Ptr_t *      vFuncs;            // functions
    Vec_Int_t *      vMemory;           // memory
    Vec_Str_t *      vCube;             // temporary
    Vec_Int_t *      vForm;             // temporary
    Vec_Ptr_t *      vVisited;          // temporary
    Vec_Ptr_t *      vLeaves;           // temporary
    // node statistics
    int              nLastGain;
    int              nNodesConsidered;
    int              nNodesRefactored;
    int              nNodesGained;
    int              nNodesBeg;
    int              nNodesEnd;
    // runtime statistics
    abctime          timeCut;
    abctime          timeTru;
    abctime          timeDcs;
    abctime          timeSop;
    abctime          timeFact;
    abctime          timeEval;
    abctime          timeRes;
    abctime          timeNtk;
    abctime          timeTotal;
};

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Returns function of the cone.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
word * Abc_NodeConeTruth( Vec_Ptr_t * vVars, Vec_Ptr_t * vFuncs, int nWordsMax, Abc_Obj_t * pRoot, Vec_Ptr_t * vLeaves, Vec_Ptr_t * vVisited )
{
    Abc_Obj_t * pNode;
    word * pTruth0, * pTruth1, * pTruth = NULL;
    int i, k, nWords = Abc_Truth6WordNum( Vec_PtrSize(vLeaves) );
    // get nodes in the cut without fanins in the DFS order
    Abc_NodeConeCollect( &pRoot, 1, vLeaves, vVisited, 0 );
    // set elementary functions
    Vec_PtrForEachEntry( Abc_Obj_t *, vLeaves, pNode, i )
        pNode->pCopy = (Abc_Obj_t *)Vec_PtrEntry( vVars, i );
    // prepare functions
    for ( i = Vec_PtrSize(vFuncs); i < Vec_PtrSize(vVisited); i++ )
        Vec_PtrPush( vFuncs, ABC_ALLOC(word, nWordsMax) );
    // compute functions for the collected nodes
    Vec_PtrForEachEntry( Abc_Obj_t *, vVisited, pNode, i )
    {
        assert( !Abc_ObjIsPi(pNode) );
        pTruth0 = (word *)Abc_ObjFanin0(pNode)->pCopy;
        pTruth1 = (word *)Abc_ObjFanin1(pNode)->pCopy;
        pTruth  = (word *)Vec_PtrEntry( vFuncs, i );
        if ( Abc_ObjFaninC0(pNode) )
        {
            if ( Abc_ObjFaninC1(pNode) )
                for ( k = 0; k < nWords; k++ )
                    pTruth[k] = ~pTruth0[k] & ~pTruth1[k];
            else
                for ( k = 0; k < nWords; k++ )
                    pTruth[k] = ~pTruth0[k] &  pTruth1[k];
        }
        else
        {
            if ( Abc_ObjFaninC1(pNode) )
                for ( k = 0; k < nWords; k++ )
                    pTruth[k] =  pTruth0[k] & ~pTruth1[k];
            else
                for ( k = 0; k < nWords; k++ )
                    pTruth[k] =  pTruth0[k] &  pTruth1[k];
        }
        pNode->pCopy = (Abc_Obj_t *)pTruth;
    }
    return pTruth;
}
int Abc_NodeConeIsConst0( word * pTruth, int nVars )
{
    int k, nWords = Abc_Truth6WordNum( nVars );
    for ( k = 0; k < nWords; k++ )
        if ( pTruth[k] )
            return 0;
    return 1;
}
int Abc_NodeConeIsConst1( word * pTruth, int nVars )
{
    int k, nWords = Abc_Truth6WordNum( nVars );
    for ( k = 0; k < nWords; k++ )
        if ( ~pTruth[k] )
            return 0;
    return 1;
}


/**Function*************************************************************

  Synopsis    [Resynthesizes the node using refactoring.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Dec_Graph_t * Abc_NodeRefactor( Abc_ManRef_t * p, Abc_Obj_t * pNode, Vec_Ptr_t * vFanins, int nMinSaved, int fUpdateLevel, int fUseZeros, int fUseDcs, int fVerbose )
{
    extern int    Dec_GraphToNetworkCount( Abc_Obj_t * pRoot, Dec_Graph_t * pGraph, int NodeMax, int LevelMax );
    int fVeryVerbose = 0;
    int nVars = Vec_PtrSize(vFanins);
    int nWordsMax = Abc_Truth6WordNum(p->nNodeSizeMax);
    Dec_Graph_t * pFForm;
    Abc_Obj_t * pFanin;
    word * pTruth;
    abctime clk;
    int i, nNodesSaved, nNodesAdded, Required;
    if ( fUseZeros )
        nMinSaved = 0;

    p->nNodesConsidered++;

    Required = fUpdateLevel? Abc_ObjRequiredLevel(pNode) : ABC_INFINITY;

    // get the function of the cut
clk = Abc_Clock();
    pTruth = Abc_NodeConeTruth( p->vVars, p->vFuncs, nWordsMax, pNode, vFanins, p->vVisited );
p->timeTru += Abc_Clock() - clk;
    if ( pTruth == NULL )
        return NULL;

    // always accept the case of constant node
    if ( Abc_NodeConeIsConst0(pTruth, nVars) || Abc_NodeConeIsConst1(pTruth, nVars) )
    {
        p->nLastGain = Abc_NodeMffcSize( pNode );
        p->nNodesGained += p->nLastGain;
        p->nNodesRefactored++;
        return Abc_NodeConeIsConst0(pTruth, nVars) ? Dec_GraphCreateConst0() : Dec_GraphCreateConst1();
    }

    // get the factored form
clk = Abc_Clock();
    pFForm = (Dec_Graph_t *)Kit_TruthToGraph( (unsigned *)pTruth, nVars, p->vMemory );
p->timeFact += Abc_Clock() - clk;

    // mark the fanin boundary 
    // (can mark only essential fanins, belonging to bNodeFunc!)
    Vec_PtrForEachEntry( Abc_Obj_t *, vFanins, pFanin, i )
        pFanin->vFanouts.nSize++;
    // label MFFC with current traversal ID
    Abc_NtkIncrementTravId( pNode->pNtk );
    nNodesSaved = Abc_NodeMffcLabelAig( pNode );
    // unmark the fanin boundary and set the fanins as leaves in the form
    Vec_PtrForEachEntry( Abc_Obj_t *, vFanins, pFanin, i )
    {
        pFanin->vFanouts.nSize--;
        Dec_GraphNode(pFForm, i)->pFunc = pFanin;
    }

    // detect how many new nodes will be added (while taking into account reused nodes)
clk = Abc_Clock();
    nNodesAdded = Dec_GraphToNetworkCount( pNode, pFForm, nNodesSaved, Required );
p->timeEval += Abc_Clock() - clk;
    // quit if there is no improvement
    //if ( nNodesAdded == -1 || (nNodesAdded == nNodesSaved && !fUseZeros) )
    if ( nNodesAdded == -1 || nNodesSaved - nNodesAdded < nMinSaved )
    {
        Dec_GraphFree( pFForm );
        return NULL;
    }

    // compute the total gain in the number of nodes
    p->nLastGain = nNodesSaved - nNodesAdded;
    p->nNodesGained += p->nLastGain;
    p->nNodesRefactored++;

    // report the progress
    if ( fVeryVerbose )
    {
        printf( "Node %6s : ",  Abc_ObjName(pNode) );
        printf( "Cone = %2d. ", vFanins->nSize );
        printf( "FF = %2d. ",   1 + Dec_GraphNodeNum(pFForm) );
        printf( "MFFC = %2d. ", nNodesSaved );
        printf( "Add = %2d. ",  nNodesAdded );
        printf( "GAIN = %2d. ", p->nLastGain );
        printf( "\n" );
    }
    return pFForm;
}


/**Function*************************************************************

  Synopsis    [Starts the resynthesis manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_ManRef_t * Abc_NtkManRefStart( int nNodeSizeMax, int nConeSizeMax, int fUseDcs, int fVerbose )
{
    Abc_ManRef_t * p;
    p = ABC_ALLOC( Abc_ManRef_t, 1 );
    memset( p, 0, sizeof(Abc_ManRef_t) );
    p->vCube        = Vec_StrAlloc( 100 );
    p->vVisited     = Vec_PtrAlloc( 100 );
    p->nNodeSizeMax = nNodeSizeMax;
    p->nConeSizeMax = nConeSizeMax;
    p->fVerbose     = fVerbose;
    p->vVars        = Vec_PtrAllocTruthTables( Abc_MaxInt(nNodeSizeMax, 6) );
    p->vFuncs       = Vec_PtrAlloc( 100 );
    p->vMemory      = Vec_IntAlloc( 1 << 16 );
    return p;
}

/**Function*************************************************************

  Synopsis    [Stops the resynthesis manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkManRefStop( Abc_ManRef_t * p )
{
    Vec_PtrFreeFree( p->vFuncs );
    Vec_PtrFree( p->vVars );
    Vec_IntFree( p->vMemory );
    Vec_PtrFree( p->vVisited );
    Vec_StrFree( p->vCube );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Stops the resynthesis manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_NtkManRefPrintStats( Abc_ManRef_t * p )
{
    printf( "Refactoring statistics:\n" );
    printf( "Nodes considered  = %8d.\n", p->nNodesConsidered );
    printf( "Nodes refactored  = %8d.\n", p->nNodesRefactored );
    printf( "Gain              = %8d. (%6.2f %%).\n", p->nNodesBeg-p->nNodesEnd, 100.0*(p->nNodesBeg-p->nNodesEnd)/p->nNodesBeg );
    ABC_PRT( "Cuts       ", p->timeCut );
    ABC_PRT( "Resynthesis", p->timeRes );
    ABC_PRT( "    BDD    ", p->timeTru );
    ABC_PRT( "    DCs    ", p->timeDcs );
    ABC_PRT( "    SOP    ", p->timeSop );
    ABC_PRT( "    FF     ", p->timeFact );
    ABC_PRT( "    Eval   ", p->timeEval );
    ABC_PRT( "AIG update ", p->timeNtk );
    ABC_PRT( "TOTAL      ", p->timeTotal );
}

/**Function*************************************************************

  Synopsis    [Performs incremental resynthesis of the AIG.]

  Description [Starting from each node, computes a reconvergence-driven cut, 
  derives BDD of the cut function, constructs ISOP, factors the ISOP, 
  and replaces the current implementation of the MFFC of the node by the 
  new factored form, if the number of AIG nodes is reduced and the total
  number of levels of the AIG network is not increated. Returns the
  number of AIG nodes saved.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Abc_NtkRefactor( Abc_Ntk_t * pNtk, int nNodeSizeMax, int nMinSaved, int nConeSizeMax, int fUpdateLevel, int fUseZeros, int fUseDcs, int fVerbose )
{
    extern int           Dec_GraphUpdateNetwork( Abc_Obj_t * pRoot, Dec_Graph_t * pGraph, int fUpdateLevel, int nGain );
    ProgressBar * pProgress;
    Abc_ManRef_t * pManRef;
    Abc_ManCut_t * pManCut;
    Dec_Graph_t * pFForm;
    Vec_Ptr_t * vFanins;
    Abc_Obj_t * pNode;
    abctime clk, clkStart = Abc_Clock();
    int i, nNodes, RetValue = 1;

    global_time = 0; 
    global_resynthesis_time = 0; 
    global_cut_time = 0; 
    global_aig_update_time = 0; 
    global_aig_converter_time = 0;  
    global_level_updates = 0;
    global_reverse_updates = 0;
    global_node_rewritten = 0;  
    global_reorder_nodes = 0;
    assert( Abc_NtkIsStrash(pNtk) );
    // cleanup the AIG
    Abc_AigCleanup((Abc_Aig_t *)pNtk->pManFunc);
    // start the managers
    pManCut = Abc_NtkManCutStart( nNodeSizeMax, nConeSizeMax, 2, 1000 );
    pManRef = Abc_NtkManRefStart( nNodeSizeMax, nConeSizeMax, fUseDcs, fVerbose );
    pManRef->vLeaves   = Abc_NtkManCutReadCutLarge( pManCut );
    // compute the reverse levels if level update is requested
    if ( fUpdateLevel )
        Abc_NtkStartReverseLevels( pNtk, 0 );

    // resynthesize each node once
    pManRef->nNodesBeg = Abc_NtkNodeNum(pNtk);
    nNodes = Abc_NtkObjNumMax(pNtk);
    pProgress = Extra_ProgressBarStart( stdout, nNodes );

    List_Ptr_Node_t * oLNode; 
    List_Ptr_t * oList = Abc_AigGetOList((Abc_Aig_t *)pNtk->pManFunc);
    oList->nSize = 0;
    
    Abc_NtkForEachNode( pNtk, pNode, i ){
        oLNode = List_PtrPushBack( oList, pNode );
        assert(oLNode != NULL);
        pNode ->oLNode = oLNode; 
    }
    i = -1;  
    int max_node_id = Abc_NtkObjNumMax(pNtk);
 
    oList->pCurItera = List_PtrFirstNode(oList);
    for (; oList->pCurItera != NULL; oList->pCurItera = oList->pCurItera ->pNext) { 
        pNode = (Abc_Obj_t *) oList->pCurItera->pData;  
        i ++;  
        // different from the condition in Abc_NtkForEachNode
        // this condition is used to avoid the nodes that may be deleted not in the nework 
        // we delete the node from AIG but do not remove the order from the linked list 
        if (pNode == NULL || !Abc_ObjIsNode(pNode)) continue;
        Extra_ProgressBarUpdate( pProgress, i, NULL );
        
        if (pNode->fHandled){
            printf("Abc_NtkRerfactor: node %d has been handled.\n", pNode->Id); 
            continue;
        } 
        // stop if all nodes have been tried once
        if ( i >= nNodes )
            break; 
   
        // skip persistant nodes
        if ( Abc_NodeIsPersistant(pNode) ){
            if (fUpdateLevel) {
                pNode->fHandled = 1;
                Abc_AigUpdateLevel_Lazy( pNode);
            } 
            continue;
        } 
        // skip the nodes with many fanouts
        if ( Abc_ObjFanoutNum(pNode) > 1000 ){
            if (fUpdateLevel) {
                pNode->fHandled = 1;
                Abc_AigUpdateLevel_Lazy( pNode);
            } 
            continue;
        }
          
        if (fUpdateLevel){
            Abc_Obj_t * pFanin0 = Abc_ObjFanin0(pNode); 
            Abc_Obj_t * pFanin1 = Abc_ObjFanin1(pNode);         
            if (pFanin0 != NULL) 
            if (!Abc_ObjIsCi(pFanin0) && pFanin0->oLNode != NULL)
                assert(pFanin0->fHandled);
            if (pFanin1 != NULL)
            if (!Abc_ObjIsCi(pFanin1) && pFanin1->oLNode != NULL)
                assert(pFanin1->fHandled); 
            if ( !pNode->fHandled )  
                Abc_AigUpdateLevel_Lazy( pNode);
        }
        // avoid persistently rewriting newly created nodes with zero gain
        if (pNode->Id > max_node_id) {
            if (fUpdateLevel)  pNode->fHandled = 1;  
            i --;
            continue;
        }
        
        
 

        // compute a reconvergence-driven cut
clk = Abc_Clock();
        vFanins = Abc_NodeFindCut( pManCut, pNode, fUseDcs );
pManRef->timeCut += Abc_Clock() - clk;
        // evaluate this cut
clk = Abc_Clock();
        pFForm = Abc_NodeRefactor( pManRef, pNode, vFanins, nMinSaved, fUpdateLevel, fUseZeros, fUseDcs, fVerbose );
pManRef->timeRes += Abc_Clock() - clk;
        if ( pFForm == NULL ){
            pNode->fHandled = 1; 
                continue;
            // acceptable replacement found, update the graph
        }
clk = Abc_Clock();
        if ( !Dec_GraphUpdateNetwork( pNode, pFForm, fUpdateLevel, pManRef->nLastGain ) )
        {
            if (fUpdateLevel)  pNode->fHandled = 1;
            Dec_GraphFree( pFForm );
            RetValue = -1;
            break;
        }
        global_node_rewritten ++; 
pManRef->timeNtk += Abc_Clock() - clk;
        Dec_GraphFree( pFForm );

        if ( fUpdateLevel ){
           pNode->fHandled = 1;
           if (!Abc_AigReplaceUpdateAff( (Abc_Aig_t *)pNtk->pManFunc)){
                RetValue = -1; 
                break; 
            }  
        } 
    }
  
    Extra_ProgressBarStop( pProgress );
pManRef->timeTotal = Abc_Clock() - clkStart;
    pManRef->nNodesEnd = Abc_NtkNodeNum(pNtk);
    

    ABC_PRT("###global_time ",                   pManRef->timeTotal);   
    ABC_PRT("###global_cut ",                    pManRef->timeCut); 
    ABC_PRT("###global_resynthesis_time",        pManRef->timeRes); 
    ABC_PRT("###global_aig_update_time",         global_aig_update_time); 
    ABC_PRT("###global_aig_converter_time ",     pManRef->timeNtk); 
      
    printf("###global_level_updates \t %ld\n",    global_level_updates);   
    printf("###global_reverse_updates \t %ld\n",  global_reverse_updates);   
    printf("###global_node_rewritten \t %ld\n",   global_node_rewritten);      
    printf("###global_reorder_nodes \t %ld\n",    global_reorder_nodes);

    // print statistics of the manager
    if ( fVerbose )
        Abc_NtkManRefPrintStats( pManRef );
    // delete the managers
    Abc_NtkManCutStop( pManCut );
    Abc_NtkManRefStop( pManRef );

      // clear the mark of fHandled nodes
    List_Ptr_Iterator_t * oLIter = (List_Ptr_Iterator_t*)ABC_ALLOC(List_Ptr_Iterator_t, 1);
    List_PtrForEach(List_Ptr_t*, oList, pNode, oLIter ){ 
        if (pNode == NULL || !Abc_ObjIsNode(pNode)) continue;
        if (pNode->Id > max_node_id) {
            pNode->fHandled = 0;
            continue;
        } 
        if ( pNode->Level != 1 + (unsigned)Abc_MaxInt( Abc_ObjFanin0(pNode)->Level, Abc_ObjFanin1(pNode)->Level ) )
            printf( "Abc_AigCheck immediately after refactor: Node \"%d\" (%d) handled has level that does not agree with the fanin levels.\n", pNode->fHandled, Abc_ObjId(pNode) );
        pNode->fHandled = 0; 
        
    }
    List_PtrClear(oList);
    ABC_FREE(oLIter);

    // put the nodes into the DFS order and reassign their IDs
    Abc_NtkReassignIds( pNtk );
//    Abc_AigCheckFaninOrder( pNtk->pManFunc );
    if ( RetValue != -1 )
    {
        // fix the levels
        if ( fUpdateLevel ){
            Abc_NtkLevel( pNtk );
            Abc_NtkStopReverseLevels( pNtk );
        }
        else
            Abc_NtkLevel( pNtk );
        // check
        if ( !Abc_NtkCheck( pNtk ) )
        {
            printf( "Abc_NtkRefactor: The network check has failed.\n" );
            return 0;
        }
    }
    return RetValue;
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

