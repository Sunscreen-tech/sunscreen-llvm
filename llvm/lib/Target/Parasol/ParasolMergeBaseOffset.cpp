//===-- ParasolRegisterInfo.cpp - Parasol Register Information ------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// Merge the offset of the address calculation into the offset field of
// instructions in a global address lowering sequence.
//
//===----------------------------------------------------------------------===//

#include "Parasol.h"
#include "ParasolTargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "parasol-merge-base-offset"
#define PARASOL_MERGE_BASE_OFFSET_NAME "Parasol Merge Base Offset"

namespace {
class ParasolMergeBaseOffsetOpt : public MachineFunctionPass {
  MachineRegisterInfo *MRI;

  bool getImmediateOffset(const MachineOperand &Operand, int32_t &Offset,
                          Register &Base);

public:
  static char ID;

  ParasolMergeBaseOffsetOpt() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &Fn) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::IsSSA);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return PARASOL_MERGE_BASE_OFFSET_NAME;
  }
};
} // namespace

char ParasolMergeBaseOffsetOpt::ID = 0;
INITIALIZE_PASS(ParasolMergeBaseOffsetOpt, DEBUG_TYPE,
                PARASOL_MERGE_BASE_OFFSET_NAME, false, false)

bool ParasolMergeBaseOffsetOpt::getImmediateOffset(
    const MachineOperand &Operand, int32_t &Offset, Register &Base) {
  MachineInstr *SrcInst = &*MRI->use_instr_begin(Operand.getReg());

  if (SrcInst->getOpcode() != Parasol::ADDrr) {
    return false;
  }

  Register LeftReg = SrcInst->getOperand(0).getReg();
  Register RightReg = SrcInst->getOperand(1).getReg();

  MachineInstr *LeftOp = &*MRI->use_instr_begin(LeftReg);
  MachineInstr *RightOp = &*MRI->use_instr_begin(RightReg);

  if (LeftOp->getOpcode() == Parasol::LoadImmediate32) {
    Offset = LeftOp->getOperand(0).getImm();
    Base = RightReg;
    return true;
  }

  if (RightOp->getOpcode() == Parasol::LoadImmediate32) {
    Offset = RightOp->getOperand(0).getImm();
    Base = LeftReg;
    return true;
  }

  return false;
}

bool ParasolMergeBaseOffsetOpt::runOnMachineFunction(MachineFunction &Fn) {
  if (skipFunction(Fn.getFunction()))
    return false;

  bool MadeChange = false;
  MRI = &Fn.getRegInfo();

  for (MachineBasicBlock &MBB : Fn) {
    LLVM_DEBUG(dbgs() << "MBB: " << MBB.getName() << "\n");
    for (MachineInstr &MInst : MBB) {
      int32_t Offset;
      Register Base;

      switch (MInst.getOpcode()) {
      default:
        continue;
      case Parasol::LOAD: {
        if (getImmediateOffset(MInst.getOperand(0), Offset, Base)) {
          MadeChange = true;
          MInst.getOperand(2).setReg(Base);
          MInst.getOperand(3).setImm(Offset);
        }
        break;
      }
      case Parasol::STORE: {
        if (getImmediateOffset(MInst.getOperand(1), Offset, Base)) {
          MadeChange = true;
          MInst.getOperand(2).setReg(Base);
          MInst.getOperand(4).setImm(Offset);
        }

        break;
      }
      }
    }
  }

  return MadeChange;
}

FunctionPass *llvm::createParasolMergeBaseOffsetPass() {
  return new ParasolMergeBaseOffsetOpt();
}