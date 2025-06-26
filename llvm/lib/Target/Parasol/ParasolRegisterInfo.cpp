//===-- ParasolRegisterInfo.cpp - Parasol Register Information ------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file contains the Parasol implementation of the TargetRegisterInfo
// class.
//
//===----------------------------------------------------------------------===//

#include "ParasolRegisterInfo.h"
#include "ParasolSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"

#define GET_REGINFO_TARGET_DESC
#include "ParasolGenRegisterInfo.inc"

#define DEBUG_TYPE "parasol-reginfo"

using namespace llvm;

ParasolRegisterInfo::ParasolRegisterInfo(const ParasolSubtarget &ST)
    : ParasolGenRegisterInfo(Parasol::X1, /*DwarfFlavour*/ 0, /*EHFlavor*/ 0,
                             /*PC*/ 0),
      Subtarget(ST), StackPtr(Parasol::X2), FramePtr(Parasol::X8) {}

const MCPhysReg *
ParasolRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return Parasol_CalleeSavedRegs_SaveList;
}

const TargetRegisterClass *
ParasolRegisterInfo::intRegClass(unsigned Size) const {
  return &Parasol::IRRegClass;
}

const uint32_t *
ParasolRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                          CallingConv::ID) const {
  return Parasol_CalleeSavedRegs_RegMask;
}

BitVector
ParasolRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  markSuperRegs(Reserved, Parasol::X2); // SP
  markSuperRegs(Reserved, Parasol::X3); // gp
  markSuperRegs(Reserved, Parasol::X4); // tp
  markSuperRegs(Reserved, Parasol::X8); // FP

  return Reserved;
}

bool ParasolRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                              int SPAdj, unsigned FIOperandNum,
                                              RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected");

  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const ParasolFrameLowering *TFI = getFrameLowering(MF);
  DebugLoc dl = MI.getDebugLoc();
  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();

  unsigned BasePtr = (TFI->hasFP(MF) ? FramePtr : StackPtr);
  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex);

  // If we're using the stack pointer to load the item, offset by the current
  // stack size
  if (!TFI->hasFP(MF))
    Offset += MF.getFrameInfo().getStackSize();

  // Fold imm into offset
  Offset += MI.getOperand(FIOperandNum + 1).getImm();

  if (MI.getOpcode() == Parasol::ADDframe) {
    // This is actually "load effective address" of the stack slot
    // instruction. We have only two-address instructions, thus we need to
    // expand it into mov + add
    const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();

    // Set this instructon to `LoadI %dst, #Offset`
    MI.setDesc(TII.get(Parasol::LoadImmediate32));
    MI.getOperand(FIOperandNum).ChangeToImmediate(Offset);
    MI.removeOperand(FIOperandNum + 1);
    Register DstReg = MI.getOperand(0).getReg();

    // Add the LoadI result to base register
    BuildMI(MBB, std::next(II), dl, TII.get(Parasol::ADDrr), DstReg)
        .addReg(DstReg)
        .addReg(BasePtr);

    return false;
  }

  MI.getOperand(FIOperandNum).ChangeToRegister(BasePtr, false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
  return false;
}

bool ParasolRegisterInfo::requiresRegisterScavenging(
    const MachineFunction &MF) const {
  return true;
}

bool ParasolRegisterInfo::requiresFrameIndexScavenging(
    const MachineFunction &MF) const {
  return true;
}

bool ParasolRegisterInfo::requiresFrameIndexReplacementScavenging(
    const MachineFunction &MF) const {
  return true;
}

bool ParasolRegisterInfo::trackLivenessAfterRegAlloc(
    const MachineFunction &MF) const {
  return true;
}

Register
ParasolRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  // llvm_unreachable("Unsupported getFrameRegister");
  return Parasol::X8;
}
