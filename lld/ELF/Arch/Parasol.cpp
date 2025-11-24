//===----------------------------------------------------------------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MathExtras.h"

using namespace lld;
using namespace lld::elf;
using namespace llvm;

namespace {

// Parasol instruction format constants
// Instructions are 64 bits. Different relocation types patch different fields.
constexpr unsigned kLoadAddrOffsetShift = 27;
constexpr unsigned kLoadAddrOffsetWidth = 32;
constexpr uint64_t kLoadAddrOffsetMask = ((1ULL << kLoadAddrOffsetWidth) - 1)
                                         << kLoadAddrOffsetShift;

constexpr unsigned kBranchOffsetShift = 8;
constexpr unsigned kBranchOffsetWidth = 32;
constexpr uint64_t kBranchOffsetMask = ((1ULL << kBranchOffsetWidth) - 1)
                                       << kBranchOffsetShift;

// Pre-computed value mask for relocation patching
constexpr uint64_t kLoadAddrValueMask = (1ULL << kLoadAddrOffsetWidth) - 1;

class ParasolTargetInfo final : public TargetInfo {
public:
  ParasolTargetInfo();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};

} // namespace

void ParasolTargetInfo::relocate(uint8_t *loc, const Relocation &rel,
                                 uint64_t val) const {
  // Parasol uses little-endian instruction encoding (64-bit instructions)
  switch (rel.type) {
  case ELF::R_Parasol_NONE:
    break;
  case ELF::R_Parasol_32: {
    // Parasol instructions are 64-bit. The 32-bit address field is at bit
    // offset 27. Read current instruction, mask in the value, write back.
    // Note: Parasol uses 32-bit addressing, so all address relocations
    // (including those for 64-bit data) use R_Parasol_32.
    if (val > UINT32_MAX)
      error(getErrorLocation(loc) + "relocation R_Parasol_32 out of range: " +
            Twine(val));
    uint64_t insn = support::endian::read64le(loc);
    insn = (insn & ~kLoadAddrOffsetMask) |
           ((val & kLoadAddrValueMask) << kLoadAddrOffsetShift);
    support::endian::write64le(loc, insn);
    break;
  }
  case ELF::R_Parasol_PC24: {
    // PC-relative relocation at bit offset 8 for branch instructions.
    // The field is 32 bits wide to accommodate the signed offset.
    int64_t signedVal = static_cast<int64_t>(val);
    if (!isInt<32>(signedVal))
      error(getErrorLocation(loc) + "relocation R_Parasol_PC24 out of range: " +
            Twine(signedVal));
    uint64_t insn = support::endian::read64le(loc);
    // Use signedVal cast to uint32_t to preserve sign bits in encoding
    uint32_t encodedOffset = static_cast<uint32_t>(signedVal);
    insn = (insn & ~kBranchOffsetMask) |
           (static_cast<uint64_t>(encodedOffset) << kBranchOffsetShift);
    support::endian::write64le(loc, insn);
    break;
  }
  default:
    llvm_unreachable("unknown relocation");
  }
}

RelExpr ParasolTargetInfo::getRelExpr(RelType type, const Symbol &s,
                                      const uint8_t *loc) const {
  switch (type) {
  case ELF::R_Parasol_NONE:
    return R_NONE;
  case ELF::R_Parasol_32:
    return R_ABS;
  case ELF::R_Parasol_PC24:
    return R_PC;
  default:
    error(getErrorLocation(loc) + "unknown relocation (" + Twine(type) +
          ") against symbol " + toString(s));
    return R_NONE;
  }
}

ParasolTargetInfo::ParasolTargetInfo() {
  pltHeaderSize = 32;
  pltEntrySize = 16;
  ipltEntrySize = 16;
}

TargetInfo *elf::getParasolTargetInfo() {
  static ParasolTargetInfo t;
  return &t;
}
