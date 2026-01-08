//===-- ParasolInstrInfo.cpp - Parasol Instruction Information ------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file contains the Parasol implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "ParasolInstrInfo.h"

#include "ParasolMachineFunction.h"
#include "ParasolTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "parasol-instrinfo"

#define GET_INSTRINFO_CTOR_DTOR
#include "ParasolGenInstrInfo.inc"

ParasolInstrInfo::ParasolInstrInfo(const ParasolSubtarget &STI)
    : ParasolGenInstrInfo(Parasol::ADJCALLSTACKDOWN, Parasol::ADJCALLSTACKUP),
      Subtarget(STI) {}

void ParasolInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator I,
                                   const DebugLoc &DL, MCRegister DestReg,
                                   MCRegister SrcReg, bool KillSrc) const {
  // Leaving this here because it might be a way for us to get the number of
  // bits of a register.
  // const TargetRegisterInfo *TRI =
  // MBB.getParent()->getSubtarget().getRegisterInfo(); const
  // TargetRegisterClass *DestRegClass = TRI->getRegClass(DestReg);
  // TRI->getRegSizeInBits(*DestRegClass);
  printf("COPY CALLED\n");

  MachineInstr *NewMI =
      BuildMI(MBB, I, MBB.findDebugLoc(I), get(Parasol::MOV), DestReg)
          .addReg(SrcReg, getKillRegState(KillSrc));

  return;
}

bool ParasolInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();

  switch (MI.getOpcode()) {
    case Parasol::PseudoCALL: {
      const GlobalValue *GV = MI.getOperand(0).getGlobal();
    
      BuildMI(MBB, MI, DL, get(Parasol::JAL))
        .addReg(Parasol::X1, RegState::Define)
        .addGlobalAddress(GV);

      MI.eraseFromParent();
      return true;
    }
    case Parasol::ADJCALLSTACKDOWN:
    case Parasol::ADJCALLSTACKUP: {
      printf("CALLED\n");
      int64_t Amount = MI.getOperand(0).getImm();
  
      if (Amount == 0) {
        MI.eraseFromParent();
        return true;
      }
  
      // ADJCALLSTACKDOWN decreases SP
      // ADJCALLSTACKUP increases SP
      if (MI.getOpcode() == Parasol::ADJCALLSTACKDOWN)
        Amount = -Amount;
  
      // Use a reserved temp register
      Register Tmp = Parasol::X1;
  
      // li tmp, Amount
      BuildMI(MBB, MI, DL, get(ParasolISD::ADD), Tmp)  // OBVIOUSLY WRONG
          .addImm(Amount);
  
      // add sp, sp, tmp
      BuildMI(MBB, MI, DL, get(ISD::ADD), Parasol::X2)
          .addReg(Parasol::X2)
          .addReg(Tmp);
  
      MI.eraseFromParent();
      return true;
    }
  }
  return false;
}
