//===- ControlDG.cpp -- Control Dependent Graph Builder-------------------------//
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
//===----------------------------------------------------------------------===//

/*
 * ControlDG.cpp
 *
 *  Created on: Sep 26, 2023
 *      Author: Xiao Cheng
 */
#include "Graphs/ControlDG.h"

using namespace SVF;

ControlDG *ControlDG::controlDg = nullptr;

void ControlDG::addControlDGEdgeFromSrcDst(const ICFGNode *src, const ICFGNode *dst, const SVFValue *pNode, s32_t branchID) {
    if (!hasControlDGNode(src->getId())) {
        addGNode(src->getId(), new ControlDGNode(src));
    }
    if (!hasControlDGNode(dst->getId())) {
        addGNode(dst->getId(), new ControlDGNode(dst));
    }
    if (!hasControlDGEdge(getControlDGNode(src->getId()), getControlDGNode(dst->getId()))) {
        ControlDGEdge *pEdge = new ControlDGEdge(getControlDGNode(src->getId()),
                                                 getControlDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
        addControlDGEdge(pEdge);
        incEdgeNum();
    } else {
        ControlDGEdge *pEdge = getControlDGEdge(getControlDGNode(src->getId()),
                                                getControlDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
    }
}