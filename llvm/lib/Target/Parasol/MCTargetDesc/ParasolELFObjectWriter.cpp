//===-- ParasolELFObjectWriter.cpp - Parasol ELF Writer -------------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/ParasolFixupKinds.h"
#include "MCTargetDesc/ParasolMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

// Update this number whenever the instruction format changes
#define ABI_VERSION 3

namespace {
class ParasolELFObjectWriter : public MCELFObjectTargetWriter {
public:
  ParasolELFObjectWriter(bool Is64Bit, uint8_t OSABI)
      : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_Parasol,
                                /*HasRelocationAddend*/ true, ABI_VERSION) {}

  ~ParasolELFObjectWriter() override = default;

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;

  bool needsRelocateWithSymbol(const MCValue &Val, const MCSymbol &Sym,
                               unsigned Type) const override;
};
} // namespace

unsigned ParasolELFObjectWriter::getRelocType(MCContext &Ctx,
                                              const MCValue &Target,
                                              const MCFixup &Fixup,
                                              bool IsPCRel) const {
  MCFixupKind Kind = Fixup.getKind();
  if (Kind >= FirstLiteralRelocationKind)
    return Kind - FirstLiteralRelocationKind;

  // Handle Parasol-specific fixups first
  unsigned FixupKind = static_cast<unsigned>(Kind);
  if (FixupKind >= FirstTargetFixupKind) {
    switch (FixupKind) {
    case Parasol::fixup_br_one_reg_imm:
    case Parasol::fixup_br_imm:
      return IsPCRel ? ELF::R_Parasol_PC24 : ELF::R_Parasol_32;
    case Parasol::fixup_load_addr:
      return ELF::R_Parasol_32; // Absolute 32-bit address for constant pool
    default:
      llvm_unreachable("Unhandled Parasol fixup kind");
    }
  }

  // Map standard fixups to ELF relocation types
  switch (Kind) {
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
    return ELF::R_Parasol_32;
  case FK_Data_8:
    return ELF::R_Parasol_32; // Use 32-bit relocation for addresses
  default:
    llvm_unreachable("Unhandled fixup kind in getRelocType");
  }
}

bool ParasolELFObjectWriter::needsRelocateWithSymbol(const MCValue &,
                                                     const MCSymbol &,
                                                     unsigned Type) const {
  return false;
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createParasolELFObjectWriter(bool Is64Bit, uint8_t OSABI) {
  return std::make_unique<ParasolELFObjectWriter>(Is64Bit, OSABI);
}
