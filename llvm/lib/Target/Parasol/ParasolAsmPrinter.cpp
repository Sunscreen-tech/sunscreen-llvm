//===-- ParasolAsmPrinter.cpp - Parasol LLVM Assembly Printer -------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format Parasol assembly language.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ParasolInstPrinter.h"
#include "ParasolInstrInfo.h"
#include "ParasolTargetMachine.h"
#include "ParasolTypeMetadata.h"
#include "TargetInfo/ParasolTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include <memory>

using namespace llvm;

#define DEBUG_TYPE "parasol-asm-printer"

namespace llvm {
class ParasolAsmPrinter : public AsmPrinter {
  std::unique_ptr<ParasolTypeMetadata> TypeMeta;

public:
  explicit ParasolAsmPrinter(TargetMachine &TM,
                             std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "Parasol Assembly Printer"; }

  bool doInitialization(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;
  void emitInstruction(const MachineInstr *MI) override;

  // This function must be present as it is internally used by the
  // auto-generated function emitPseudoExpansionLowering to expand pseudo
  // instruction
  void EmitToStreamer(MCStreamer &S, const MCInst &Inst);
  // Auto-generated function in ParasolGenMCPseudoLowering.inc
  bool emitPseudoExpansionLowering(MCStreamer &OutStreamer,
                                   const MachineInstr *MI);

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;

private:
  void LowerInstruction(const MachineInstr *MI, MCInst &OutMI) const;
  MCOperand LowerOperand(const MachineOperand &MO) const;
  MCOperand LowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym) const;
};
} // namespace llvm

// Simple pseudo-instructions have their lowering (with expansion to real
// instructions) auto-generated.
#include "ParasolGenMCPseudoLowering.inc"
void ParasolAsmPrinter::EmitToStreamer(MCStreamer &S, const MCInst &Inst) {
  AsmPrinter::EmitToStreamer(*OutStreamer, Inst);
}

// Bit sizes for constant pool load instructions
static constexpr unsigned kLoadSize64 = 64;
static constexpr unsigned kLoadSize128 = 128;

void ParasolAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Handle LoadConstantPool pseudo-instructions
  unsigned Opc = MI->getOpcode();
  if (Opc == Parasol::LoadConstantPool64 ||
      Opc == Parasol::LoadConstantPool128) {
    // These pseudo-instructions need to be expanded to a sequence:
    // 1. LoadImmediate32 dst, 0  - Load 0 into dst (to use as base address)
    //    Note: Parasol does not have a dedicated zero register, so we must
    //    explicitly load zero before using it as a base address.
    // 2. LOAD dst, dst, size, symbol - Load from (0 + symbol) into dst
    //    The symbol offset becomes the effective address via relocation.
    //
    // We use dst as both the temporary for 0 and the final destination.
    // This works because LOAD reads addr before writing to dst.

    const unsigned DstReg = MI->getOperand(0).getReg();

    // Emit: LoadImmediate32 dst, 0
    MCInst LDI;
    LDI.setOpcode(Parasol::LoadImmediate32);
    LDI.addOperand(MCOperand::createReg(DstReg));
    LDI.addOperand(MCOperand::createImm(0));
    EmitToStreamer(*OutStreamer, LDI);

    // Emit: LOAD dst, dst, size, symbol
    MCInst LoadInst;
    LoadInst.setOpcode(Parasol::LOAD);

    // Operand 0: destination register (val)
    LoadInst.addOperand(MCOperand::createReg(DstReg));

    // Operand 1: address register (addr) - use dst which now contains 0
    LoadInst.addOperand(MCOperand::createReg(DstReg));

    // Operand 2: size (in bits)
    const unsigned Size =
        (Opc == Parasol::LoadConstantPool64) ? kLoadSize64 : kLoadSize128;
    LoadInst.addOperand(MCOperand::createImm(Size));

    // Operand 3: offset - constant pool symbol (this will create a fixup)
    const MachineOperand &CPOp = MI->getOperand(1);
    assert(CPOp.isCPI() && "Expected constant pool index operand");
    unsigned CPIdx = CPOp.getIndex();
    MCSymbol *CPSym = GetCPISymbol(CPIdx);
    const MCExpr *Expr = MCSymbolRefExpr::create(CPSym, OutContext);
    LoadInst.addOperand(MCOperand::createExpr(Expr));

    EmitToStreamer(*OutStreamer, LoadInst);
    return;
  }

  // Do any auto-generated pseudo lowerings.
  if (emitPseudoExpansionLowering(*OutStreamer, MI))
    return;

  MCInst TmpInst;
  LowerInstruction(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void ParasolAsmPrinter::LowerInstruction(const MachineInstr *MI,
                                         MCInst &OutMI) const {
  OutMI.setOpcode(MI->getOpcode());

  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp = LowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

MCOperand ParasolAsmPrinter::LowerOperand(const MachineOperand &MO) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands.
    if (MO.isImplicit()) {
      break;
    }
    return MCOperand::createReg(MO.getReg());

  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());

  case MachineOperand::MO_MachineBasicBlock:
    return LowerSymbolOperand(MO, MO.getMBB()->getSymbol());

  case MachineOperand::MO_GlobalAddress:
    return LowerSymbolOperand(MO, getSymbol(MO.getGlobal()));

  case MachineOperand::MO_BlockAddress:
    return LowerSymbolOperand(MO, GetBlockAddressSymbol(MO.getBlockAddress()));

  case MachineOperand::MO_ExternalSymbol:
    return LowerSymbolOperand(MO, GetExternalSymbolSymbol(MO.getSymbolName()));

  case MachineOperand::MO_ConstantPoolIndex:
    return LowerSymbolOperand(MO, GetCPISymbol(MO.getIndex()));

  case MachineOperand::MO_RegisterMask:
    break;

  default:
    report_fatal_error("unknown operand type");
  }

  return MCOperand();
}

MCOperand ParasolAsmPrinter::LowerSymbolOperand(const MachineOperand &MO,
                                                MCSymbol *Sym) const {
  MCContext &Ctx = OutContext;

  const MCExpr *Expr =
      MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, Ctx);

  if (!MO.isJTI() && !MO.isMBB() && MO.getOffset())
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(MO.getOffset(), Ctx), Ctx);

  return MCOperand::createExpr(Expr);
}

bool ParasolAsmPrinter::doInitialization(Module &M) {
  AsmPrinter::doInitialization(M);

  // Create type metadata handler and process the module for fhe_program funcs
  TypeMeta = std::make_unique<ParasolTypeMetadata>(this);
  TypeMeta->processModule(M);

  return false;
}

void ParasolAsmPrinter::emitEndOfAsmFile(Module &M) {
  // Emit the .parasol_meta section with type info for fhe_program functions
  if (TypeMeta)
    TypeMeta->endModule();
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeParasolAsmPrinter() {
  RegisterAsmPrinter<ParasolAsmPrinter> X(getTheParasolTarget());
}

bool ParasolAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                        const char *ExtraCode,
                                        raw_ostream &OS) {
  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << ParasolInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, OS);
    return false;
  case MachineOperand::MO_BlockAddress: {
    MCSymbol *Sym = GetBlockAddressSymbol(MO.getBlockAddress());
    Sym->print(OS, MAI);
    return false;
  }
  default:
    break;
  }

  return true;
}