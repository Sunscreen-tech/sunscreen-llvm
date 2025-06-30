//===-- ParasolISelDAGToDAG.cpp - A Dag to Dag Inst Selector for Parasol --===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Parasol target.
//
//===----------------------------------------------------------------------===//

#include "ParasolISelDAGToDAG.h"
#include "ParasolSubtarget.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

using namespace llvm;

#define DEBUG_TYPE "parasol-isel"

char ParasolDAGToDAGISel::ID = 0;

bool ParasolDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &static_cast<const ParasolSubtarget &>(MF.getSubtarget());
  return SelectionDAGISel::runOnMachineFunction(MF);
}

void ParasolDAGToDAGISel::Select(SDNode *Node) {
  SDLoc dl(Node);

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // Select the default instruction
  SelectCode(Node);
}

bool ParasolDAGToDAGISel::SelectAddrFrameIndex(SDValue Addr, SDValue &Base,
                                               SDValue &Offset) {
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i32);
    return true;
  }

  return false;
}

// Select a frame index.
bool ParasolDAGToDAGISel::SelectFrameAddrRegImm(SDValue Addr, SDValue &Base,
                                                SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset)) {
    return true;
  }

  if (!CurDAG->isBaseWithConstantOffset(Addr)) {
    return false;
  }

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<32>(CVal)) {
      Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
      Offset = CurDAG->getTargetConstant(CVal, SDLoc(Addr), MVT::i32);
      return true;
    }
  }

  return false;
}
