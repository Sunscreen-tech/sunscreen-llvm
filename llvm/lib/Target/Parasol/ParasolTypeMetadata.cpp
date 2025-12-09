//===-- ParasolTypeMetadata.cpp - Parasol Type Metadata Emission ----------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file implements the emission of function type metadata to the
// .parasol_meta ELF section for functions marked with [[clang::fhe_program]].
//
//===----------------------------------------------------------------------===//

#include "ParasolTypeMetadata.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "parasol-type-metadata"

//===----------------------------------------------------------------------===//
// Helper Functions
//===----------------------------------------------------------------------===//

/// Generate a canonical type name for an integer type based on bit width and
/// signedness. Returns names like "int32_t", "uint8_t", "bool", etc.
static std::string getIntTypeName(unsigned BitWidth, bool IsSigned) {
  if (BitWidth == 1)
    return "bool";

  if (BitWidth == 8 || BitWidth == 16 || BitWidth == 32 || BitWidth == 64 ||
      BitWidth == 128) {
    std::string Name = IsSigned ? "int" : "uint";
    Name += std::to_string(BitWidth) + "_t";
    return Name;
  }

  // For non-standard bit widths, use i<width> or u<width>
  std::string Name = IsSigned ? "i" : "u";
  Name += std::to_string(BitWidth);
  return Name;
}

/// Check if a 64-bit value fits in a 32-bit field and report a fatal error
/// with the given message if it does not.
static uint32_t checkOverflow32(uint64_t Value, const char *Msg) {
  if (Value > UINT32_MAX)
    report_fatal_error(Msg);
  return static_cast<uint32_t>(Value);
}

//===----------------------------------------------------------------------===//
// ParasolStringTable
//===----------------------------------------------------------------------===//

uint32_t ParasolStringTable::addString(StringRef S) {
  // Check for existing entry
  auto It = StringToOffset.find(S);
  if (It != StringToOffset.end())
    return It->second;

  // Add new entry with overflow checking
  uint32_t Offset = Size;
  StringToOffset[S] = Offset;
  Strings.push_back(S.str());
  uint64_t NewSize = static_cast<uint64_t>(Size) + S.size() + 1;
  Size = checkOverflow32(NewSize, "string table size exceeds 32-bit limit");
  return Offset;
}

//===----------------------------------------------------------------------===//
// ParasolTypeEntry base class
//===----------------------------------------------------------------------===//

void ParasolTypeEntry::emitType(MCStreamer &OS) const {
  OS.emitInt32(TypeData.NameOff);
  OS.emitInt32(TypeData.Info);
  OS.emitInt32(TypeData.SizeOrType);
}

//===----------------------------------------------------------------------===//
// ParasolTypeInt
//===----------------------------------------------------------------------===//

ParasolTypeInt::ParasolTypeInt(uint32_t BitWidth, bool IsSigned, StringRef Name,
                               ParasolStringTable &StrTab)
    : ParasolTypeEntry(ParasolMeta::KIND_INT) {
  TypeData.NameOff = StrTab.addString(Name);
  TypeData.SizeOrType = (BitWidth + 7) / 8; // Size in bytes
  // Encoding: bits 0-7 = bit width, bit 8 = signed flag
  IntEncoding = BitWidth | (IsSigned ? ParasolMeta::IntSignedBit : 0);
}

void ParasolTypeInt::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
  OS.emitInt32(IntEncoding);
}

//===----------------------------------------------------------------------===//
// ParasolTypePtr
//===----------------------------------------------------------------------===//

void ParasolTypePtr::completeType(ParasolTypeMetadata &Meta) {
  // Skip if we already have a pre-computed pointee type from debug info
  if (HasPrecomputedPointeeType)
    return;

  PointeeTypeId = Meta.getOrCreateTypeId(PointeeTy);
  TypeData.SizeOrType = PointeeTypeId;
}

void ParasolTypePtr::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
}

//===----------------------------------------------------------------------===//
// ParasolTypeArray
//===----------------------------------------------------------------------===//

void ParasolTypeArray::completeType(ParasolTypeMetadata &Meta) {
  ElemTypeId = Meta.getOrCreateTypeId(ElemTy);
  // Use cached uint32 type for index
  IndexTypeId = Meta.getUint32TypeId();
  TypeData.setVlen(0); // Not used for arrays
}

void ParasolTypeArray::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
  OS.emitInt32(ElemTypeId);
  OS.emitInt32(IndexTypeId);
  OS.emitInt32(checkOverflow32(NumElems, "array size exceeds 32-bit limit"));
}

//===----------------------------------------------------------------------===//
// ParasolTypeStruct
//===----------------------------------------------------------------------===//

ParasolTypeStruct::ParasolTypeStruct(const StructType *S, StringRef Name,
                                     ParasolStringTable &StrTab)
    : ParasolTypeEntry(ParasolMeta::KIND_STRUCT), STy(S) {
  TypeData.NameOff = StrTab.addString(Name);
  TypeData.setVlen(STy->getNumElements());

  // Initialize member entries (type IDs filled in completeType)
  Members.resize(STy->getNumElements());
  for (unsigned I = 0; I < STy->getNumElements(); ++I) {
    // Use field index as name for now (struct field names not in LLVM IR)
    std::string FieldName = "field" + std::to_string(I);
    Members[I].NameOff = StrTab.addString(FieldName);
    Members[I].TypeId = 0; // Filled in completeType
    Members[I].Offset = 0; // Filled in completeType
  }
}

void ParasolTypeStruct::completeType(ParasolTypeMetadata &Meta) {
  const DataLayout &DL = Meta.Asm->getDataLayout();
  // LLVM's getStructLayout() takes a non-const StructType* but doesn't modify
  // it. The const_cast is safe here.
  const StructLayout *SL = DL.getStructLayout(const_cast<StructType *>(STy));

  TypeData.SizeOrType =
      checkOverflow32(SL->getSizeInBytes(), "struct size exceeds 32-bit limit");

  for (unsigned I = 0; I < STy->getNumElements(); ++I) {
    Members[I].TypeId = Meta.getOrCreateTypeId(STy->getElementType(I));
    Members[I].Offset = checkOverflow32(SL->getElementOffset(I),
                                        "struct offset exceeds 32-bit limit");
  }
}

void ParasolTypeStruct::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
  for (const auto &Member : Members) {
    OS.emitInt32(Member.NameOff);
    OS.emitInt32(Member.TypeId);
    OS.emitInt32(Member.Offset);
  }
}

//===----------------------------------------------------------------------===//
// ParasolTypeFuncProto
//===----------------------------------------------------------------------===//

void ParasolTypeFuncProto::initializeParams(const Function *Func,
                                            ParasolStringTable &StrTab,
                                            ArrayRef<uint32_t> ParamTypeIds) {
  // Variadic functions are not supported in FHE programs
  if (FTy->isVarArg())
    report_fatal_error("variadic functions not supported in fhe_program");

  TypeData.NameOff = 0; // Anonymous
  TypeData.setVlen(FTy->getNumParams());

  // Initialize parameter entries
  Params.resize(FTy->getNumParams());
  if (Func) {
    unsigned I = 0;
    for (const Argument &Arg : Func->args()) {
      Params[I].NameOff = StrTab.addString(Arg.getName());
      Params[I].TypeId = ParamTypeIds.empty() ? 0 : ParamTypeIds[I];
      ++I;
    }
  } else {
    for (unsigned I = 0; I < FTy->getNumParams(); ++I) {
      std::string ParamName = "arg" + std::to_string(I);
      Params[I].NameOff = StrTab.addString(ParamName);
      Params[I].TypeId = ParamTypeIds.empty() ? 0 : ParamTypeIds[I];
    }
  }
}

ParasolTypeFuncProto::ParasolTypeFuncProto(const FunctionType *F,
                                           const Function *Func,
                                           ParasolStringTable &StrTab)
    : ParasolTypeEntry(ParasolMeta::KIND_FUNC_PROTO), FTy(F) {
  initializeParams(Func, StrTab, {});
}

ParasolTypeFuncProto::ParasolTypeFuncProto(const FunctionType *F,
                                           const Function *Func,
                                           ParasolStringTable &StrTab,
                                           uint32_t RetTypeId,
                                           ArrayRef<uint32_t> ParamTypeIds)
    : ParasolTypeEntry(ParasolMeta::KIND_FUNC_PROTO), FTy(F),
      ReturnTypeId(RetTypeId),
      PrecomputedParamTypes(ParamTypeIds.begin(), ParamTypeIds.end()) {
  if (ParamTypeIds.size() != FTy->getNumParams())
    report_fatal_error("parameter type count mismatch in fhe_program");

  initializeParams(Func, StrTab, ParamTypeIds);
  TypeData.SizeOrType = RetTypeId; // Set return type immediately
}

void ParasolTypeFuncProto::completeType(ParasolTypeMetadata &Meta) {
  // If we have pre-computed types from debug info, skip the lookup
  if (!PrecomputedParamTypes.empty()) {
    // Return type and param types are already set in the constructor
    return;
  }

  ReturnTypeId = Meta.getOrCreateTypeId(FTy->getReturnType());
  TypeData.SizeOrType = ReturnTypeId;

  for (unsigned I = 0; I < FTy->getNumParams(); ++I) {
    Params[I].TypeId = Meta.getOrCreateTypeId(FTy->getParamType(I));
  }
}

void ParasolTypeFuncProto::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
  // Return type ID is already in SizeOrType
  for (const auto &Param : Params) {
    OS.emitInt32(Param.NameOff);
    OS.emitInt32(Param.TypeId);
  }
}

//===----------------------------------------------------------------------===//
// ParasolTypeFunc
//===----------------------------------------------------------------------===//

ParasolTypeFunc::ParasolTypeFunc(StringRef Name, uint32_t ProtoId,
                                 ParasolStringTable &StrTab)
    : ParasolTypeEntry(ParasolMeta::KIND_FUNC), ProtoTypeId(ProtoId) {
  TypeData.NameOff = StrTab.addString(Name);
  TypeData.SizeOrType = ProtoTypeId;
}

void ParasolTypeFunc::emitType(MCStreamer &OS) const {
  ParasolTypeEntry::emitType(OS);
}

//===----------------------------------------------------------------------===//
// ParasolTypeMetadata
//===----------------------------------------------------------------------===//

uint32_t ParasolTypeMetadata::addType(std::unique_ptr<ParasolTypeEntry> Entry) {
  uint32_t Id = checkOverflow32(TypeEntries.size() + 1,
                                "type count exceeds 32-bit limit");
  Entry->setId(Id);
  TypeEntries.push_back(std::move(Entry));
  return Id;
}

uint32_t ParasolTypeMetadata::getVoidTypeId() {
  if (VoidTypeId == 0) {
    auto Entry = std::make_unique<ParasolTypeVoid>();
    VoidTypeId = addType(std::move(Entry));
  }
  return VoidTypeId;
}

uint32_t ParasolTypeMetadata::getUint32TypeId() {
  if (Uint32TypeId == 0) {
    auto Entry =
        std::make_unique<ParasolTypeInt>(32, false, "uint32_t", StringTable);
    Uint32TypeId = addType(std::move(Entry));
  }
  return Uint32TypeId;
}

uint32_t ParasolTypeMetadata::getOrCreateTypeId(const Type *Ty) {
  // Check cache
  auto It = TypeToIdMap.find(Ty);
  if (It != TypeToIdMap.end())
    return It->second;

  uint32_t TypeId = 0;

  if (Ty->isVoidTy()) {
    TypeId = getVoidTypeId();
  } else if (Ty->isIntegerTy()) {
    unsigned BitWidth = Ty->getIntegerBitWidth();
    // LLVM integers are signless - default to unsigned when no debug info
    bool IsSigned = false;
    std::string Name = getIntTypeName(BitWidth, IsSigned);
    auto Entry =
        std::make_unique<ParasolTypeInt>(BitWidth, IsSigned, Name, StringTable);
    TypeId = addType(std::move(Entry));
  } else if (Ty->isPointerTy()) {
    // LLVM uses opaque pointers (ptr), so pointee type info is not available.
    // All pointers are represented as void* in the metadata. This is a known
    // limitation - consumers should use debug info if precise pointer types
    // are needed.
    auto Entry =
        std::make_unique<ParasolTypePtr>(Type::getVoidTy(Ty->getContext()));
    TypeId = addType(std::move(Entry));
  } else if (auto *ATy = dyn_cast<ArrayType>(Ty)) {
    auto Entry = std::make_unique<ParasolTypeArray>(ATy->getElementType(),
                                                    ATy->getNumElements());
    TypeId = addType(std::move(Entry));
  } else if (auto *STy = dyn_cast<StructType>(Ty)) {
    StringRef Name = STy->hasName() ? STy->getName() : "";
    auto Entry = std::make_unique<ParasolTypeStruct>(STy, Name, StringTable);
    TypeId = addType(std::move(Entry));
  } else if (auto *FTy = dyn_cast<FunctionType>(Ty)) {
    auto Entry =
        std::make_unique<ParasolTypeFuncProto>(FTy, nullptr, StringTable);
    TypeId = addType(std::move(Entry));
  } else {
    // Unknown type - treat as void
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Unknown type kind\n");
    TypeId = getVoidTypeId();
  }

  TypeToIdMap[Ty] = TypeId;
  return TypeId;
}

//===----------------------------------------------------------------------===//
// DIType visitor methods (for debug info based type extraction)
//===----------------------------------------------------------------------===//

uint32_t ParasolTypeMetadata::visitTypeEntry(const DIType *DTy,
                                             LLVMContext &Ctx) {
  if (!DTy)
    return getVoidTypeId();

  // Check cache first
  if (auto Cached = getCachedDIType(DTy))
    return *Cached;

  // Dispatch to appropriate visitor based on DIType kind
  if (auto *BTy = dyn_cast<DIBasicType>(DTy))
    return visitBasicType(BTy);
  else if (auto *DerivedTy = dyn_cast<DIDerivedType>(DTy))
    return visitDerivedType(DerivedTy, Ctx);
  else if (auto *CompositeTy = dyn_cast<DICompositeType>(DTy))
    return visitCompositeType(CompositeTy);
  else if (auto *SubroutineTy = dyn_cast<DISubroutineType>(DTy))
    return visitSubroutineType(SubroutineTy, nullptr);

  // Unknown DIType kind - fall back to void
  LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Unknown DIType kind\n");
  return getVoidTypeId();
}

uint32_t ParasolTypeMetadata::visitBasicType(const DIBasicType *BTy) {
  // Check cache
  if (auto Cached = getCachedDIType(BTy))
    return *Cached;

  unsigned BitWidth = BTy->getSizeInBits();
  unsigned Encoding = BTy->getEncoding();

  // Determine signedness from DWARF encoding
  bool IsSigned = false;
  switch (Encoding) {
  case dwarf::DW_ATE_signed:
  case dwarf::DW_ATE_signed_char:
    IsSigned = true;
    break;
  case dwarf::DW_ATE_unsigned:
  case dwarf::DW_ATE_unsigned_char:
  case dwarf::DW_ATE_boolean:
    IsSigned = false;
    break;
  default:
    // For unknown encodings, default to unsigned
    IsSigned = false;
    break;
  }

  std::string Name = getIntTypeName(BitWidth, IsSigned);
  auto Entry =
      std::make_unique<ParasolTypeInt>(BitWidth, IsSigned, Name, StringTable);
  uint32_t TypeId = addType(std::move(Entry));
  DITypeToIdMap[BTy] = TypeId;
  return TypeId;
}

uint32_t ParasolTypeMetadata::visitDerivedType(const DIDerivedType *DTy,
                                               LLVMContext &Ctx) {
  // Check cache
  if (auto Cached = getCachedDIType(DTy))
    return *Cached;

  auto Tag = DTy->getTag();
  auto *BaseTy = DTy->getBaseType();

  // Helper to cache and return a type ID
  auto cacheAndReturn = [&](uint32_t Id) {
    DITypeToIdMap[DTy] = Id;
    return Id;
  };

  // Helper to visit the base type or return void if null
  auto visitBaseOrVoid = [&]() {
    return BaseTy ? visitTypeEntry(BaseTy, Ctx) : getVoidTypeId();
  };

  switch (Tag) {
  case dwarf::DW_TAG_pointer_type: {
    // For pointers, visit the pointee type and create a pointer type
    uint32_t PointeeTypeId = visitBaseOrVoid();
    auto Entry =
        std::make_unique<ParasolTypePtr>(Type::getVoidTy(Ctx), PointeeTypeId);
    return cacheAndReturn(addType(std::move(Entry)));
  }
  case dwarf::DW_TAG_const_type:
  case dwarf::DW_TAG_volatile_type:
  case dwarf::DW_TAG_restrict_type:
  case dwarf::DW_TAG_typedef:
  case dwarf::DW_TAG_member:
    // For qualifiers, typedefs, and members, strip and visit the base type
    return cacheAndReturn(visitBaseOrVoid());
  default:
    // Unknown derived type - fall back to base type or void
    return cacheAndReturn(visitBaseOrVoid());
  }
}

uint32_t ParasolTypeMetadata::visitCompositeType(const DICompositeType *CTy) {
  // Check cache
  if (auto Cached = getCachedDIType(CTy))
    return *Cached;

  unsigned Tag = CTy->getTag();
  uint32_t TypeId = 0;

  switch (Tag) {
  case dwarf::DW_TAG_structure_type:
  case dwarf::DW_TAG_union_type: {
    // For now, we can't fully reconstruct LLVM StructType from DICompositeType
    // Fall back to creating types from LLVM IR types
    // This is a limitation when only debug info is available
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Struct/union from debug info "
                         "not fully supported, using void*\n");
    TypeId = getVoidTypeId();
    DITypeToIdMap[CTy] = TypeId;
    break;
  }
  case dwarf::DW_TAG_array_type: {
    // Array types - get element type and size
    // For arrays, we'd need to extract the array size from subrange
    // This is complex, so for now fall back to LLVM IR type handling
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Array from debug info not "
                         "fully supported\n");
    TypeId = getVoidTypeId();
    DITypeToIdMap[CTy] = TypeId;
    break;
  }
  default:
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Unknown composite type tag\n");
    TypeId = getVoidTypeId();
    DITypeToIdMap[CTy] = TypeId;
    break;
  }

  return TypeId;
}

uint32_t ParasolTypeMetadata::visitSubroutineType(const DISubroutineType *STy,
                                                  const Function *F) {
  // Check cache
  if (auto Cached = getCachedDIType(STy))
    return *Cached;

  auto Elements = STy->getTypeArray();
  if (Elements.empty())
    return getVoidTypeId();

  // First element is return type, rest are parameters
  auto *ReturnDIType = Elements[0];

  if (!F) {
    // Without a function, we can't create a complete FuncProto
    return getVoidTypeId();
  }

  auto &Ctx = F->getContext();

  // Visit return type first to get its type ID
  uint32_t ReturnTypeId = visitTypeEntry(ReturnDIType, Ctx);

  // Visit all parameter types from debug info and collect their type IDs
  std::vector<uint32_t> ParamTypeIds;
  ParamTypeIds.reserve(Elements.size() - 1);
  for (unsigned I = 1; I < Elements.size(); ++I)
    ParamTypeIds.push_back(visitTypeEntry(Elements[I], Ctx));

  auto *FTy = F->getFunctionType();

  // Verify parameter count matches
  if (ParamTypeIds.size() != FTy->getNumParams()) {
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: parameter count mismatch "
                         "between debug info and LLVM IR\n");
    // Fall back to creating without pre-computed types
    auto ProtoEntry =
        std::make_unique<ParasolTypeFuncProto>(FTy, F, StringTable);
    uint32_t TypeId = addType(std::move(ProtoEntry));
    DITypeToIdMap[STy] = TypeId;
    return TypeId;
  }

  // Create function prototype with pre-computed type IDs
  auto ProtoEntry = std::make_unique<ParasolTypeFuncProto>(
      FTy, F, StringTable, ReturnTypeId, ParamTypeIds);

  uint32_t TypeId = addType(std::move(ProtoEntry));
  DITypeToIdMap[STy] = TypeId;
  return TypeId;
}

void ParasolTypeMetadata::recordFheProgramFunction(const Function &F,
                                                   uint32_t ProtoId) {
  // Create function type entry
  auto FuncEntry =
      std::make_unique<ParasolTypeFunc>(F.getName(), ProtoId, StringTable);
  uint32_t FuncId = addType(std::move(FuncEntry));

  // Record function info. Note: StringTable.addString() deduplicates, so
  // calling it again here is harmless (the name was already added by
  // ParasolTypeFunc constructor).
  ParasolFuncInfo FI;
  FI.Symbol = Asm->getSymbol(&F);
  FI.NameOff = StringTable.addString(F.getName());
  FI.TypeId = FuncId;
  FheProgramFuncs.push_back(FI);
}

void ParasolTypeMetadata::processModule(const Module &M) {
  LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Processing module\n");

  for (const Function &F : M) {
    // Skip declarations and non-fhe_program functions
    if (F.isDeclaration() || !F.hasFnAttribute(Attribute::FheProgram))
      continue;

    LLVM_DEBUG(dbgs() << "  Found fhe_program function: " << F.getName()
                      << "\n");

    // Try to use debug info if available for better type information
    auto *SP = F.getSubprogram();
    bool UsedDebugInfo = false;

    if (SP && SP->getType()) {
      LLVM_DEBUG(dbgs() << "  Using debug info for " << F.getName() << "\n");

      auto &Ctx = F.getContext();

      // Pre-visit parameter types from retained nodes to extract signedness
      for (const auto *DN : SP->getRetainedNodes()) {
        if (const auto *DV = dyn_cast<DILocalVariable>(DN)) {
          if (DV->getArg())
            visitTypeEntry(DV->getType(), Ctx);
        }
      }

      // Process the function signature via debug info
      uint32_t ProtoId = visitSubroutineType(SP->getType(), &F);

      if (ProtoId != getVoidTypeId()) {
        UsedDebugInfo = true;
        recordFheProgramFunction(F, ProtoId);
      }
    }

    // Fall back to LLVM IR types if debug info not available or failed
    if (!UsedDebugInfo) {
      LLVM_DEBUG(dbgs() << "  Falling back to LLVM IR types for " << F.getName()
                        << "\n");

      auto *FTy = F.getFunctionType();
      auto ProtoEntry =
          std::make_unique<ParasolTypeFuncProto>(FTy, &F, StringTable);
      uint32_t ProtoId = addType(std::move(ProtoEntry));

      recordFheProgramFunction(F, ProtoId);
    }
  }

  LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: Found " << FheProgramFuncs.size()
                    << " fhe_program functions\n");
}

void ParasolTypeMetadata::emitParasolMetaSection() {
  if (FheProgramFuncs.empty()) {
    LLVM_DEBUG(dbgs() << "ParasolTypeMetadata: No fhe_program functions, "
                         "skipping section emission\n");
    return;
  }

  MCStreamer &OS = *Asm->OutStreamer;
  MCContext &Ctx = OS.getContext();

  // Create the section
  MCSectionELF *Sec = Ctx.getELFSection(".parasol_meta", ELF::SHT_PROGBITS, 0);
  Sec->setAlignment(Align(4));
  OS.switchSection(Sec);

  // Calculate section sizes
  uint32_t TypeLen = 0;
  for (const auto &Entry : TypeEntries)
    TypeLen += Entry->getSize();

  uint32_t StrLen = StringTable.getSize();
  uint32_t FuncLen = FheProgramFuncs.size() * ParasolMeta::FuncInfoSize;

  // Emit header
  OS.AddComment("Magic: PARA");
  OS.emitInt32(ParasolMeta::Magic);

  OS.AddComment("Version");
  OS.emitInt8(ParasolMeta::Version);

  OS.AddComment("Flags");
  OS.emitInt8(0);

  OS.AddComment("Reserved");
  OS.emitInt16(0);

  OS.AddComment("TypeOff");
  OS.emitInt32(0); // Types start immediately after header

  OS.AddComment("TypeLen");
  OS.emitInt32(TypeLen);

  OS.AddComment("StrOff");
  OS.emitInt32(TypeLen);

  OS.AddComment("StrLen");
  OS.emitInt32(StrLen);

  OS.AddComment("FuncOff");
  OS.emitInt32(TypeLen + StrLen);

  OS.AddComment("FuncLen");
  OS.emitInt32(FuncLen);

  // Emit type entries
  OS.AddComment("--- Type Section ---");
  for (const auto &Entry : TypeEntries)
    Entry->emitType(OS);

  // Emit string table
  OS.AddComment("--- String Section ---");
  for (const auto &Str : StringTable.getStrings()) {
    OS.emitBytes(StringRef(Str));
    OS.emitInt8(0); // Null terminator
  }

  // Emit function info entries
  OS.AddComment("--- Function Section ---");
  for (const auto &FI : FheProgramFuncs) {
    OS.emitInt32(FI.NameOff);
    OS.emitInt32(FI.TypeId);
    // Emit symbol reference (will be resolved by linker)
    OS.emitSymbolValue(FI.Symbol, 4);
  }
}

void ParasolTypeMetadata::endModule() {
  // Complete all type cross-references using a work-queue approach.
  // completeType() may add new types via getOrCreateTypeId(), so we track
  // how many we've processed and continue until all are done.
  size_t Processed = 0;
  while (Processed < TypeEntries.size()) {
    TypeEntries[Processed]->completeType(*this);
    ++Processed;
  }

  emitParasolMetaSection();
}
