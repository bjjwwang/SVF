//===- FlowSensitiveStat.cpp -- Statistics for flow-sensitive pointer analysis-//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 * FlowSensitiveStat.cpp
 *
 *  Created on: 27/11/2013
 *      Author: yesen
 */

#include "WPA/Andersen.h"
#include "WPA/WPAStat.h"
#include "WPA/FlowSensitive.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Clear statistics
 */
void FlowSensitiveStat::clearStat()
{
    _NumOfNullPtr = 0;
    _NumOfConstantPtr = 0;
    _NumOfBlackholePtr = 0;
    _MaxPtsSize = _MaxTopLvlPtsSize = 0;
    _AvgPtsSize = _AvgTopLvlPtsSize = 0;
    _AvgAddrTakenVarPtsSize = _NumOfAddrTakeVar = 0;
    _MaxAddrTakenVarPts = 0;
    _TotalPtsSize = 0;

    for (int i=IN_SET; i<=OUT_SET; i++)
    {
        /// SVFG nodes
        _NumOfSVFGNodesHaveInOut[i] = 0;
        _NumOfFormalInSVFGNodesHaveInOut[i] = 0;
        _NumOfFormalOutSVFGNodesHaveInOut[i] = 0;
        _NumOfActualInSVFGNodesHaveInOut[i] = 0;
        _NumOfActualOutSVFGNodesHaveInOut[i] = 0;
        _NumOfLoadSVFGNodesHaveInOut[i] = 0;
        _NumOfStoreSVFGNodesHaveInOut[i] = 0;
        _NumOfMSSAPhiSVFGNodesHaveInOut[i] = 0;

        /// SVFIR nodes.
        _NumOfVarHaveINOUTPts[i] = 0;
        _NumOfVarHaveEmptyINOUTPts[i] = 0;
        _NumOfVarHaveINOUTPtsInFormalIn[i] = 0;
        _NumOfVarHaveINOUTPtsInFormalOut[i] = 0;;
        _NumOfVarHaveINOUTPtsInActualIn[i] = 0;
        _NumOfVarHaveINOUTPtsInActualOut[i] = 0;
        _NumOfVarHaveINOUTPtsInLoad[i] = 0;
        _NumOfVarHaveINOUTPtsInStore[i] = 0;
        _NumOfVarHaveINOUTPtsInMSSAPhi[i] = 0;
        _PotentialNumOfVarHaveINOUTPts[i] = 0;

        _MaxInOutPtsSize[i] = 0;
        _AvgInOutPtsSize[i] = 0;
    }
}

/*!
 * Start statistics
 */
void FlowSensitiveStat::performStat()
{
    assert(SVFUtil::isa<FlowSensitive>(fspta) && "not an flow-sensitive pta pass!! what else??");
    endClk();

    clearStat();

    SVFIR* pag = fspta->getPAG();

    // stat null ptr number
    statNullPtr();

    // stat points-to set information
    statPtsSize();

    // stat address-taken variables' points-to
    statAddrVarPtsSize();

    u32_t fiObjNumber = 0;
    u32_t fsObjNumber = 0;
    Set<NodeID> nodeSet;
    for (SVFIR::const_iterator nodeIt = pag->begin(), nodeEit = pag->end(); nodeIt != nodeEit; nodeIt++)
    {
        NodeID nodeId = nodeIt->first;
        PAGNode* pagNode = nodeIt->second;
        if(SVFUtil::isa<ObjVar>(pagNode))
        {
            const BaseObjVar* baseObj = pag->getBaseObject(nodeId);
            NodeID baseId = baseObj->getId();
            if (nodeSet.insert(baseId).second)
            {
                if (baseObj->isFieldInsensitive())
                    fiObjNumber++;
                else
                    fsObjNumber++;
            }
        }
    }

    PTNumStatMap["FIObjNum"] = fiObjNumber;
    PTNumStatMap["FSObjNum"] = fsObjNumber;

    unsigned numOfCopy = 0;
    unsigned numOfStore = 0;
    SVFG::iterator svfgNodeIt = fspta->svfg->begin();
    SVFG::iterator svfgNodeEit = fspta->svfg->end();
    for (; svfgNodeIt != svfgNodeEit; ++svfgNodeIt)
    {
        SVFGNode* svfgNode = svfgNodeIt->second;
        if (SVFUtil::isa<CopySVFGNode>(svfgNode))
            numOfCopy++;
        else if (SVFUtil::isa<StoreSVFGNode>(svfgNode))
            numOfStore++;
    }

    PTAStat::performStat();

    timeStatMap["TotalTime"] = (endTime - startTime)/TIMEINTERVAL;
    timeStatMap["SolveTime"] = fspta->solveTime;
    timeStatMap["SCCTime"] = fspta->sccTime;
    timeStatMap["ProcessTime"] = fspta->processTime;
    timeStatMap["PropagationTime"] = fspta->propagationTime;
    timeStatMap["DirectPropaTime"] = fspta->directPropaTime;
    timeStatMap["IndirectPropaTime"] = fspta->indirectPropaTime;
    timeStatMap["Strong/WeakUpdTime"] = fspta->updateTime;
    timeStatMap["AddrTime"] = fspta->addrTime;
    timeStatMap["CopyTime"] = fspta->copyTime;
    timeStatMap["GepTime"] = fspta->gepTime;
    timeStatMap["LoadTime"] = fspta->loadTime;
    timeStatMap["StoreTime"] = fspta->storeTime;
    timeStatMap["UpdateCGTime"] = fspta->updateCallGraphTime;
    timeStatMap["PhiTime"] = fspta->phiTime;

    PTNumStatMap["TotalPointers"] = pag->getValueNodeNum();
    PTNumStatMap["TotalObjects"] = pag->getObjectNodeNum();

    PTNumStatMap["Pointers"] = pag->getValueNodeNum();
    PTNumStatMap["MemObjects"] = pag->getObjectNodeNum();
    PTNumStatMap["DummyFieldPtrs"] = pag->getFieldValNodeNum();
    PTNumStatMap["FieldObjs"] = pag->getFieldObjNodeNum();

    PTNumStatMap["CopysNum"] = numOfCopy;
    PTNumStatMap["StoresNum"] = numOfStore;

    PTNumStatMap["SolveIterations"] = fspta->numOfIteration;

    PTNumStatMap["IndEdgeSolved"] = fspta->getNumOfResolvedIndCallEdge();

    PTNumStatMap["NullPointer"] = _NumOfNullPtr;
    PTNumStatMap["PointsToConstPtr"] = _NumOfConstantPtr;
    PTNumStatMap["PointsToBlkPtr"] = _NumOfBlackholePtr;

    PTNumStatMap["StrongUpdates"] = fspta->svfgHasSU.count();

    /// SVFG nodes.
    PTNumStatMap["SNodesHaveIN"] = _NumOfSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["SNodesHaveOUT"] = _NumOfSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["FI_SNodesHaveIN"] = _NumOfFormalInSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["FI_SNodesHaveOUT"] = _NumOfFormalInSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["FO_SNodesHaveIN"] = _NumOfFormalOutSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["FO_SNodesHaveOUT"] = _NumOfFormalOutSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["AI_SNodesHaveIN"] = _NumOfActualInSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["AI_SNodesHaveOUT"] = _NumOfActualInSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["AO_SNodesHaveIN"] = _NumOfActualOutSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["AO_SNodesHaveOUT"] = _NumOfActualOutSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["LD_SNodesHaveIN"] = _NumOfLoadSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["LD_SNodesHaveOUT"] = _NumOfLoadSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["ST_SNodesHaveIN"] = _NumOfStoreSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["ST_SNodesHaveOUT"] = _NumOfStoreSVFGNodesHaveInOut[OUT_SET];

    PTNumStatMap["PHI_SNodesHaveIN"] = _NumOfMSSAPhiSVFGNodesHaveInOut[IN_SET];
    PTNumStatMap["PHI_SNodesHaveOUT"] = _NumOfMSSAPhiSVFGNodesHaveInOut[OUT_SET];

    /*-----------------------------------------------------*/
    // SVFIR nodes.
    PTNumStatMap["VarHaveIN"] = _NumOfVarHaveINOUTPts[IN_SET];
    PTNumStatMap["VarHaveOUT"] = _NumOfVarHaveINOUTPts[OUT_SET];

    PTNumStatMap["PotentialVarHaveIN"] = _PotentialNumOfVarHaveINOUTPts[IN_SET];
    PTNumStatMap["PotentialVarHaveOUT"] = _PotentialNumOfVarHaveINOUTPts[OUT_SET];

    PTNumStatMap["VarHaveEmptyIN"] = _NumOfVarHaveEmptyINOUTPts[IN_SET];
    PTNumStatMap["VarHaveEmptyOUT"] = _NumOfVarHaveEmptyINOUTPts[OUT_SET];

    PTNumStatMap["VarHaveIN_FI"] = _NumOfVarHaveINOUTPtsInFormalIn[IN_SET];
    PTNumStatMap["VarHaveOUT_FI"] = _NumOfVarHaveINOUTPtsInFormalIn[OUT_SET];

    PTNumStatMap["VarHaveIN_FO"] = _NumOfVarHaveINOUTPtsInFormalOut[IN_SET];
    PTNumStatMap["VarHaveOUT_FO"] = _NumOfVarHaveINOUTPtsInFormalOut[OUT_SET];

    PTNumStatMap["VarHaveIN_AI"] = _NumOfVarHaveINOUTPtsInActualIn[IN_SET];
    PTNumStatMap["VarHaveOUT_AI"] = _NumOfVarHaveINOUTPtsInActualIn[OUT_SET];

    PTNumStatMap["VarHaveIN_AO"] = _NumOfVarHaveINOUTPtsInActualOut[IN_SET];
    PTNumStatMap["VarHaveOUT_AO"] = _NumOfVarHaveINOUTPtsInActualOut[OUT_SET];

    PTNumStatMap["VarHaveIN_LD"] = _NumOfVarHaveINOUTPtsInLoad[IN_SET];
    PTNumStatMap["VarHaveOUT_LD"] = _NumOfVarHaveINOUTPtsInLoad[OUT_SET];

    PTNumStatMap["VarHaveIN_ST"] = _NumOfVarHaveINOUTPtsInStore[IN_SET];
    PTNumStatMap["VarHaveOUT_ST"] = _NumOfVarHaveINOUTPtsInStore[OUT_SET];

    PTNumStatMap["VarHaveIN_PHI"] = _NumOfVarHaveINOUTPtsInMSSAPhi[IN_SET];
    PTNumStatMap["VarHaveOUT_PHI"] = _NumOfVarHaveINOUTPtsInMSSAPhi[OUT_SET];

    PTNumStatMap["MaxPtsSize"] = _MaxPtsSize;
    PTNumStatMap["MaxTopLvlPtsSize"] = _MaxTopLvlPtsSize;
    PTNumStatMap["MaxINPtsSize"] = _MaxInOutPtsSize[IN_SET];
    PTNumStatMap["MaxOUTPtsSize"] = _MaxInOutPtsSize[OUT_SET];

    timeStatMap["AvgPtsSize"] = _AvgPtsSize;
    timeStatMap["AvgTopLvlPtsSize"] = _AvgTopLvlPtsSize;

    PTNumStatMap["NumOfAddrTakenVar"] = _NumOfAddrTakeVar;
    timeStatMap["AvgAddrTakenVarPts"] = (_NumOfAddrTakeVar == 0) ?
                                        0 : ((double)_AvgAddrTakenVarPtsSize / _NumOfAddrTakeVar);
    PTNumStatMap["MaxAddrTakenVarPts"] = _MaxAddrTakenVarPts;

    timeStatMap["AvgINPtsSize"] = _AvgInOutPtsSize[IN_SET];
    timeStatMap["AvgOUTPtsSize"] = _AvgInOutPtsSize[OUT_SET];

    PTNumStatMap["ProcessedAddr"] = fspta->numOfProcessedAddr;
    PTNumStatMap["ProcessedCopy"] = fspta->numOfProcessedCopy;
    PTNumStatMap["ProcessedGep"] = fspta->numOfProcessedGep;
    PTNumStatMap["ProcessedLoad"] = fspta->numOfProcessedLoad;
    PTNumStatMap["ProcessedStore"] = fspta->numOfProcessedStore;
    PTNumStatMap["ProcessedPhi"] = fspta->numOfProcessedPhi;
    PTNumStatMap["ProcessedAParam"] = fspta->numOfProcessedActualParam;
    PTNumStatMap["ProcessedFRet"] = fspta->numOfProcessedFormalRet;
    PTNumStatMap["ProcessedMSSANode"] = fspta->numOfProcessedMSSANode;

    PTNumStatMap["NumOfNodesInSCC"] = fspta->numOfNodesInSCC;
    PTNumStatMap["MaxSCCSize"] = fspta->maxSCCSize;
    PTNumStatMap["NumOfSCC"] = fspta->numOfSCC;
    timeStatMap["AverageSCCSize"] = (fspta->numOfSCC == 0) ? 0 :
                                    ((double)fspta->numOfNodesInSCC / fspta->numOfSCC);

    PTAStat::printStat("Flow-Sensitive Pointer Analysis Statistics");
}

void FlowSensitiveStat::statNullPtr()
{
    _NumOfNullPtr = 0;
    for (SVFIR::iterator iter = fspta->getPAG()->begin(), eiter = fspta->getPAG()->end();
            iter != eiter; ++iter)
    {
        NodeID pagNodeId = iter->first;
        PAGNode* pagNode = iter->second;
        SVFStmt::SVFStmtSetTy& inComingStore = pagNode->getIncomingEdges(SVFStmt::Store);
        SVFStmt::SVFStmtSetTy& outGoingLoad = pagNode->getOutgoingEdges(SVFStmt::Load);
        if (inComingStore.empty()==false || outGoingLoad.empty()==false)
        {
            ///TODO: change the condition here to fetch the points-to set
            const PointsTo& pts = fspta->getPts(pagNodeId);
            if(fspta->containBlackHoleNode(pts))
            {
                _NumOfConstantPtr++;
            }
            if(fspta->containConstantNode(pts))
            {
                _NumOfBlackholePtr++;
            }
            if(pts.empty())
            {
                std::string str;
                std::stringstream  rawstr(str);
                if (!SVFUtil::isa<DummyValVar>(pagNode) && !SVFUtil::isa<DummyObjVar>(pagNode))
                {
                    // if a pointer is in dead function, we do not care
                    if(pagNode->ptrInUncalledFunction() == false)
                    {
                        _NumOfNullPtr++;
                        rawstr << "##Null Pointer : (NodeID " << pagNode->getId()
                               << ") PtrName:" << pagNode->getName();
                        writeWrnMsg(rawstr.str());
                    }
                }
                else
                {
                    _NumOfNullPtr++;
                    rawstr << "##Null Pointer : (NodeID " << pagNode->getId();
                    writeWrnMsg(rawstr.str());
                }
            }
        }
    }
}

/*!
 * Points-to size
 */
void FlowSensitiveStat::statPtsSize()
{
    // TODO: needs to be made to work for persistent.
    if (SVFUtil::isa<FlowSensitive::MutDFPTDataTy>(fspta->getPTDataTy()))
    {
        // stat of IN_SET set
        statInOutPtsSize(fspta->getDFInputMap(), IN_SET);
        // stat of OUT_SET set
        statInOutPtsSize(fspta->getDFOutputMap(), OUT_SET);
    }

    /// get points-to set size information for top-level pointers.
    u32_t totalValidTopLvlPointers = 0;
    u32_t topTopLvlPtsSize = 0;
    for (SVFIR::iterator iter = fspta->getPAG()->begin(), eiter = fspta->getPAG()->end();
            iter != eiter; ++iter)
    {
        NodeID node = iter->first;
        if (fspta->getPAG()->isValidTopLevelPtr(iter->second) == false)
            continue;
        u32_t size = fspta->getPts(node).count();

        totalValidTopLvlPointers++;
        topTopLvlPtsSize+=size;

        if(size > _MaxPtsSize)	_MaxPtsSize = size;

        if (size > _MaxTopLvlPtsSize)	_MaxTopLvlPtsSize = size;
    }

    if (totalValidTopLvlPointers != 0)
        _AvgTopLvlPtsSize = (double)topTopLvlPtsSize/totalValidTopLvlPointers;

    _TotalPtsSize += topTopLvlPtsSize;
    u32_t totalPointer = totalValidTopLvlPointers + _NumOfVarHaveINOUTPts[IN_SET] + _NumOfVarHaveINOUTPts[OUT_SET];
    if (totalPointer != 0)
        _AvgPtsSize = (double) _TotalPtsSize / totalPointer;
}

void FlowSensitiveStat::statInOutPtsSize(const DFInOutMap& data, ENUM_INOUT inOrOut)
{
    // Get number of nodes which have IN_SET/OUT_SET set
    _NumOfSVFGNodesHaveInOut[inOrOut] = data.size();

    u32_t inOutPtsSize = 0;
    DFInOutMap::const_iterator it = data.begin();
    DFInOutMap::const_iterator eit = data.end();
    for (; it != eit; ++it)
    {
        const SVFGNode* node = fspta->svfg->getSVFGNode(it->first);

        // Count number of SVFG nodes have IN_SET/OUT_SET set.
        if (SVFUtil::isa<FormalINSVFGNode>(node))
            _NumOfFormalInSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<FormalOUTSVFGNode>(node))
            _NumOfFormalOutSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<ActualINSVFGNode>(node))
            _NumOfActualInSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<ActualOUTSVFGNode>(node))
            _NumOfActualOutSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<LoadSVFGNode>(node))
            _NumOfLoadSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<StoreSVFGNode>(node))
            _NumOfStoreSVFGNodesHaveInOut[inOrOut]++;
        else if (SVFUtil::isa<MSSAPHISVFGNode>(node))
            _NumOfMSSAPhiSVFGNodesHaveInOut[inOrOut]++;
        else
            assert(false && "unexpected node have IN_SET/OUT_SET set");

        /*-----------------------------------------------------*/

        // Count SVFIR nodes and their points-to set size.
        const PtsMap& cptsMap = it->second;
        PtsMap::const_iterator ptsIt = cptsMap.begin();
        PtsMap::const_iterator ptsEit = cptsMap.end();
        for (; ptsIt != ptsEit; ++ptsIt)
        {
            if (ptsIt->second.empty())
            {
                _NumOfVarHaveEmptyINOUTPts[inOrOut]++;
                continue;
            }

            u32_t ptsNum = ptsIt->second.count();	/// points-to target number

            // Only node with non-empty points-to set are counted.
            _NumOfVarHaveINOUTPts[inOrOut]++;

            if (SVFUtil::isa<FormalINSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInFormalIn[inOrOut]++;
            else if (SVFUtil::isa<FormalOUTSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInFormalOut[inOrOut]++;
            else if (SVFUtil::isa<ActualINSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInActualIn[inOrOut]++;
            else if (SVFUtil::isa<ActualOUTSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInActualOut[inOrOut]++;
            else if (SVFUtil::isa<LoadSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInLoad[inOrOut]++;
            else if (SVFUtil::isa<StoreSVFGNode>(node))
                _NumOfVarHaveINOUTPtsInStore[inOrOut]++;
            else if (SVFUtil::isa<MSSAPHISVFGNode>(node))
                _NumOfVarHaveINOUTPtsInMSSAPhi[inOrOut]++;
            else
                assert(false && "unexpected node have IN_SET/OUT_SET set");

            inOutPtsSize += ptsNum;

            if (ptsNum > _MaxInOutPtsSize[inOrOut])
                _MaxInOutPtsSize[inOrOut] = ptsNum;

            if (ptsNum > _MaxPtsSize) _MaxPtsSize = ptsNum;
        }
    }

    if (_NumOfVarHaveINOUTPts[inOrOut] != 0)
        _AvgInOutPtsSize[inOrOut] = (double)inOutPtsSize / _NumOfVarHaveINOUTPts[inOrOut];

    _TotalPtsSize += inOutPtsSize;

    // How many IN_SET/OUT_SET PTSs could we have *potentially* had?
    // l'-o->l, l''-o->l, ..., means there is a possibility of 1 IN_SET PTS.
    // *p = q && { o } in pts_ander(p) means there is a possibility of 1 OUT_SET PTS.
    // For OUTs at stores, we must also account for WU/SUs.
    const SVFG *svfg = fspta->svfg;
    for (SVFG::const_iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const SVFGNode *sn = it->second;

        // Unique objects coming into s.
        NodeBS incomingObjects;
        for (const SVFGEdge *e : sn->getInEdges())
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;
            for (NodeID o : ie->getPointsTo()) incomingObjects.set(o);
        }

        _PotentialNumOfVarHaveINOUTPts[IN_SET] += incomingObjects.count();

        if (const StoreSVFGNode *store = SVFUtil::dyn_cast<StoreSVFGNode>(sn))
        {
            NodeID p = store->getPAGDstNodeID();
            // Reuse incomingObjects; what's already in there will be propagated forwarded
            // as a WU/SU, and what's not (first defined at the store), will be added.
            for (NodeID o : fspta->ander->getPts(p)) incomingObjects.set(o);

            _PotentialNumOfVarHaveINOUTPts[OUT_SET] += incomingObjects.count();
        }
    }
}

/*!
 * Points-to size
 */
void FlowSensitiveStat::statAddrVarPtsSize()
{
    SVFG::SVFGNodeIDToNodeMapTy::const_iterator it = fspta->svfg->begin();
    SVFG::SVFGNodeIDToNodeMapTy::const_iterator eit = fspta->svfg->end();
    for (; it != eit; ++it)
    {
        const SVFGNode* node = it->second;
        if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
        {
            calculateAddrVarPts(store->getPAGDstNodeID(), store);
        }
    }
}

/*!
 * Points-to size
 */
void FlowSensitiveStat::calculateAddrVarPts(NodeID pointer, const SVFGNode* svfg_node)
{
    const PointsTo& pts = fspta->getPts(pointer);
    _NumOfAddrTakeVar += pts.count();
    PointsTo::iterator ptsIt = pts.begin();
    PointsTo::iterator ptsEit = pts.end();
    for (; ptsIt != ptsEit; ++ptsIt)
    {
        const NodeID ptd = *ptsIt;

        const PointsTo& cpts = fspta->getDFOutPtsSet(svfg_node, ptd);
        _AvgAddrTakenVarPtsSize += cpts.count();
        if (cpts.count() > _MaxAddrTakenVarPts)
            _MaxAddrTakenVarPts = cpts.count();
    }
}


