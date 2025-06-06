//===- SVFBugReport.cpp -- Base class for statistics---------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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


//
// Created by JoelYang on 2023/4/19.
//

#include "Util/SVFBugReport.h"
#include <cassert>
#include "Util/cJSON.h"
#include "Util/SVFUtil.h"
#include <sstream>
#include <fstream>

using namespace std;
using namespace SVF;

const std::map<GenericBug::BugType, std::string> GenericBug::BugType2Str =
{
    {GenericBug::FULLBUFOVERFLOW, "Full Buffer Overflow"},
    {GenericBug::PARTIALBUFOVERFLOW, "Partial Buffer Overflow"},
    {GenericBug::NEVERFREE, "Never Free"},
    {GenericBug::PARTIALLEAK, "Partial Leak"},
    {GenericBug::FILENEVERCLOSE, "File Never Close"},
    {GenericBug::FILEPARTIALCLOSE, "File Partial Close"},
    {GenericBug::DOUBLEFREE, "Double Free"},
    {GenericBug::FULLNULLPTRDEREFERENCE, "Full Null Ptr Dereference"},
    {GenericBug::PARTIALNULLPTRDEREFERENCE, "Partial Null Ptr Dereference"}
};

const std::string GenericBug::getLoc() const
{
    const SVFBugEvent&sourceInstEvent = bugEventStack.at(bugEventStack.size() -1);
    return sourceInstEvent.getEventLoc();
}

const std::string GenericBug::getFuncName() const
{
    const SVFBugEvent&sourceInstEvent = bugEventStack.at(bugEventStack.size() -1);
    return sourceInstEvent.getFuncName();
}

cJSON *BufferOverflowBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    cJSON *allocLB = cJSON_CreateNumber(allocLowerBound);
    cJSON *allocUB = cJSON_CreateNumber(allocUpperBound);
    cJSON *accessLB = cJSON_CreateNumber(accessLowerBound);
    cJSON *accessUB = cJSON_CreateNumber(accessUpperBound);

    cJSON_AddItemToObject(bugDescription, "AllocLowerBound", allocLB);
    cJSON_AddItemToObject(bugDescription, "AllocUpperBound", allocUB);
    cJSON_AddItemToObject(bugDescription, "AccessLowerBound", accessLB);
    cJSON_AddItemToObject(bugDescription, "AccessUpperBound", accessUB);

    return bugDescription;
}

void BufferOverflowBug::printBugToTerminal() const
{
    stringstream bugInfo;
    if(FullBufferOverflowBug::classof(this))
    {
        SVFUtil::errs() << SVFUtil::bugMsg1("\t Full Overflow :") <<  " accessing at : ("
                        << GenericBug::getLoc() << ")\n";

    }
    else
    {
        SVFUtil::errs() << SVFUtil::bugMsg1("\t Partial Overflow :") <<  " accessing at : ("
                        << GenericBug::getLoc() << ")\n";
    }
    bugInfo << "\t\t  allocate size : [" << allocLowerBound << ", " << allocUpperBound << "], ";
    bugInfo << "access size : [" << accessLowerBound << ", " << accessUpperBound << "]\n";
    SVFUtil::errs() << "\t\t Info : \n" << bugInfo.str();
    SVFUtil::errs() << "\t\t Events : \n";

    for(auto event : bugEventStack)
    {
        switch(event.getEventType())
        {
        case SVFBugEvent::CallSite:
        {
            SVFUtil::errs() << "\t\t  callsite at : ( " << event.getEventLoc() << " )\n";
            break;
        }
        default:
        {
            // TODO: implement more events when needed
            break;
        }
        }
    }
}

cJSON * NeverFreeBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    return bugDescription;
}

void NeverFreeBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg1("\t NeverFree :") <<  " memory allocation at : ("
                    << GenericBug::getLoc() << ")\n";
}

cJSON * PartialLeakBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    cJSON *pathInfo = cJSON_CreateArray();
    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        cJSON *newBranch = cJSON_CreateObject();
        cJSON *branchLoc = cJSON_Parse((*eventIt).getEventLoc().c_str());
        if(branchLoc == nullptr) branchLoc = cJSON_CreateObject();

        cJSON *branchCondition = cJSON_CreateString((*eventIt).getEventDescription().c_str());

        cJSON_AddItemToObject(newBranch, "BranchLoc", branchLoc);
        cJSON_AddItemToObject(newBranch, "BranchCond", branchCondition);

        cJSON_AddItemToArray(pathInfo, newBranch);
    }

    cJSON_AddItemToObject(bugDescription, "ConditionalFreePath", pathInfo);

    return bugDescription;
}

void PartialLeakBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg2("\t PartialLeak :") <<  " memory allocation at : ("
                    << GenericBug::getLoc() << ")\n";

    SVFUtil::errs() << "\t\t conditional free path: \n";
    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        SVFUtil::errs() << "\t\t  --> (" << (*eventIt).getEventLoc() << "|" << (*eventIt).getEventDescription() << ") \n";
    }
    SVFUtil::errs() << "\n";
}

cJSON * DoubleFreeBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();

    cJSON *pathInfo = cJSON_CreateArray();
    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        cJSON *newBranch = cJSON_CreateObject();
        cJSON *branchLoc = cJSON_Parse((*eventIt).getEventLoc().c_str());
        if(branchLoc == nullptr) branchLoc = cJSON_CreateObject();
        cJSON *branchCondition = cJSON_CreateString((*eventIt).getEventDescription().c_str());

        cJSON_AddItemToObject(newBranch, "BranchLoc", branchLoc);
        cJSON_AddItemToObject(newBranch, "BranchCond", branchCondition);

        cJSON_AddItemToArray(pathInfo, newBranch);
    }
    cJSON_AddItemToObject(bugDescription, "DoubleFreePath", pathInfo);

    return bugDescription;
}

void DoubleFreeBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg2("\t Double Free :") <<  " memory allocation at : ("
                    << GenericBug::getLoc() << ")\n";

    SVFUtil::errs() << "\t\t double free path: \n";
    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        SVFUtil::errs() << "\t\t  --> (" << (*eventIt).getEventLoc() << "|" << (*eventIt).getEventDescription() << ") \n";
    }
    SVFUtil::errs() << "\n";
}

cJSON * FileNeverCloseBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    return bugDescription;
}

void FileNeverCloseBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg1("\t FileNeverClose :") <<  " file open location at : ("
                    << GenericBug::getLoc() << ")\n";
}

cJSON * FilePartialCloseBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();

    cJSON *pathInfo = cJSON_CreateArray();

    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        cJSON *newBranch = cJSON_CreateObject();
        cJSON *branchLoc = cJSON_Parse((*eventIt).getEventLoc().c_str());
        if(branchLoc == nullptr) branchLoc = cJSON_CreateObject();
        cJSON *branchCondition = cJSON_CreateString((*eventIt).getEventDescription().c_str());

        cJSON_AddItemToObject(newBranch, "BranchLoc", branchLoc);
        cJSON_AddItemToObject(newBranch, "BranchCond", branchCondition);

        cJSON_AddItemToArray(pathInfo, newBranch);
    }
    cJSON_AddItemToObject(bugDescription, "ConditionalFileClosePath", pathInfo);

    return bugDescription;
}

void FilePartialCloseBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg2("\t PartialFileClose :") <<  " file open location at : ("
                    << GenericBug::getLoc() << ")\n";

    SVFUtil::errs() << "\t\t conditional file close path: \n";
    auto lastBranchEventIt = bugEventStack.end() - 1;
    for(auto eventIt = bugEventStack.begin(); eventIt != lastBranchEventIt; eventIt++)
    {
        SVFUtil::errs() << "\t\t  --> (" << (*eventIt).getEventLoc() << "|" << (*eventIt).getEventDescription() << ") \n";
    }
    SVFUtil::errs() << "\n";
}

cJSON *FullNullPtrDereferenceBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    return bugDescription;
}

void FullNullPtrDereferenceBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg2("\t FullNullPtrDereference :") <<  " dereference at : ("
                    << GenericBug::getLoc() << ")\n";
}

cJSON *PartialNullPtrDereferenceBug::getBugDescription() const
{
    cJSON *bugDescription = cJSON_CreateObject();
    return bugDescription;
}

void PartialNullPtrDereferenceBug::printBugToTerminal() const
{
    SVFUtil::errs() << SVFUtil::bugMsg2("\t PartialNullPtrDereference :") <<  " dereference at : ("
                    << GenericBug::getLoc() << ")\n";
}

const std::string SVFBugEvent::getFuncName() const
{
    return eventInst->getFun()->getName();
}

const std::string SVFBugEvent::getEventLoc() const
{
    return eventInst->getSourceLoc();
}

const std::string SVFBugEvent::getEventDescription() const
{
    switch(getEventType())
    {
    case SVFBugEvent::Branch:
    {
        if (typeAndInfoFlag & BRANCHFLAGMASK)
        {
            return "True";
        }
        else
        {
            return "False";
        }
        break;
    }
    case SVFBugEvent::CallSite:
    {
        std::string description("calls ");
        assert(SVFUtil::isa<CallICFGNode>(eventInst) && "not a call ICFGNode?");
        const FunObjVar *callee = SVFUtil::cast<CallICFGNode>(eventInst)->getCalledFunction();
        if(callee == nullptr)
        {
            description += "<unknown>";
        }
        else
        {
            description += callee->getName();
        }
        return description;
        break;
    }
    case SVFBugEvent::SourceInst:
    {
        return "None";
    }
    default:
    {
        assert(false && "No such type of event!");
        abort();
    }
    }
}

SVFBugReport::~SVFBugReport()
{
    for(auto bugIt: bugSet)
    {
        delete bugIt;
    }
}

void SVFBugReport::dumpToJsonFile(const std::string& filePath) const
{
    std::map<u32_t, std::string> eventType2Str =
    {
        {SVFBugEvent::CallSite, "call site"},
        {SVFBugEvent::Caller, "caller"},
        {SVFBugEvent::Loop, "loop"},
        {SVFBugEvent::Branch, "branch"}
    };

    ofstream jsonFile(filePath, ios::out);

    jsonFile << "{\n";

    /// Add defects
    jsonFile << "\"Defects\": [\n";
    size_t commaCounter = bugSet.size() - 1;
    for (auto bugPtr : bugSet)
    {
        cJSON *singleDefect = cJSON_CreateObject();

        /// Add bug information to JSON
        cJSON *bugType = cJSON_CreateString(
                             GenericBug::BugType2Str.at(bugPtr->getBugType()).c_str());
        cJSON_AddItemToObject(singleDefect, "DefectType", bugType);

        cJSON *bugLoc = cJSON_Parse(bugPtr->getLoc().c_str());
        if (bugLoc == nullptr)
        {
            bugLoc = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(singleDefect, "Location", bugLoc);

        cJSON *bugFunction = cJSON_CreateString(
                                 bugPtr->getFuncName().c_str());
        cJSON_AddItemToObject(singleDefect, "Function", bugFunction);

        cJSON_AddItemToObject(singleDefect, "Description",
                              bugPtr->getBugDescription());

        /// Add event information to JSON
        cJSON *eventList = cJSON_CreateArray();
        const GenericBug::EventStack &bugEventStack = bugPtr->getEventStack();
        if (BufferOverflowBug::classof(bugPtr))
        {
            // Add only when bug is context sensitive
            for (const SVFBugEvent &event : bugEventStack)
            {
                if (event.getEventType() == SVFBugEvent::SourceInst)
                {
                    continue;
                }

                cJSON *singleEvent = cJSON_CreateObject();
                // Event type
                cJSON *eventType = cJSON_CreateString(
                                       eventType2Str[event.getEventType()].c_str());
                cJSON_AddItemToObject(singleEvent, "EventType", eventType);
                // Function name
                cJSON *eventFunc = cJSON_CreateString(
                                       event.getFuncName().c_str());
                cJSON_AddItemToObject(singleEvent, "Function", eventFunc);
                // Event loc
                cJSON *eventLoc = cJSON_Parse(event.getEventLoc().c_str());
                if (eventLoc == nullptr)
                {
                    eventLoc = cJSON_CreateObject();
                }
                cJSON_AddItemToObject(singleEvent, "Location", eventLoc);
                // Event description
                cJSON *eventDescription = cJSON_CreateString(
                                              event.getEventDescription().c_str());
                cJSON_AddItemToObject(singleEvent, "Description", eventDescription);

                cJSON_AddItemToArray(eventList, singleEvent);
            }
        }
        cJSON_AddItemToObject(singleDefect, "Events", eventList);

        /// Dump single bug to JSON string and write to file
        char *singleDefectStr = cJSON_Print(singleDefect);
        jsonFile << singleDefectStr;
        if (commaCounter != 0)
        {
            jsonFile << ",\n";
        }
        commaCounter--;

        /// Destroy the cJSON object
        cJSON_Delete(singleDefect);
    }
    jsonFile << "\n],\n";

    /// Add program information
    jsonFile << "\"Time\": " << time << ",\n";
    jsonFile << "\"Memory\": \"" << mem << "\",\n";
    jsonFile << "\"Coverage\": " << coverage << "\n";

    jsonFile << "}";
    jsonFile.close();
}
