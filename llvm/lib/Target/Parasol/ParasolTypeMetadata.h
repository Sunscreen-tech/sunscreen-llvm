//===-- ParasolTypeMetadata.h - Parasol Type Metadata Emission --*- C++ -*-===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//
//
// This file contains support for emitting function type metadata to the
// .parasol_meta ELF section for functions marked with [[clang::fhe_program]].
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_PARASOL_PARASOLTYPEMETADATA_H
#define LLVM_LIB_TARGET_PARASOL_PARASOLTYPEMETADATA_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace llvm {

class Function;
class MCStreamer;
class MCSymbol;
class Module;
class StructType;

/// Binary format constants for .parasol_meta section
namespace ParasolMeta {

/// Magic number: "PARA" in little-endian
constexpr uint32_t Magic = 0x41524150;

/// Current format version
constexpr uint8_t Version = 1;

/// Header size in bytes
constexpr uint32_t HeaderSize = 32;

/// Common type entry size (without extra data)
constexpr uint32_t CommonTypeSize = 12;

/// Array extra data size
constexpr uint32_t ArrayExtraSize = 12;

/// Struct member size
constexpr uint32_t MemberSize = 12;

/// Function parameter size
constexpr uint32_t ParamSize = 8;

/// FuncInfo entry size
constexpr uint32_t FuncInfoSize = 12;

/// Bit flag for signed integer encoding
constexpr uint32_t IntSignedBit = 0x100;

/// Type kinds
enum TypeKind : uint8_t {
  KIND_VOID = 0,
  KIND_INT = 1,
  KIND_PTR = 2,
  KIND_ARRAY = 3,
  KIND_STRUCT = 4,
  KIND_FUNC = 5,
  KIND_FUNC_PROTO = 6,
};

/// Section header structure
struct Header {
  uint32_t Magic;    // 0x41524150 ("PARA")
  uint8_t Version;   // Format version (1)
  uint8_t Flags;     // Reserved flags
  uint16_t Reserved; // Padding
  uint32_t TypeOff;  // Offset to type section (from header end)
  uint32_t TypeLen;  // Length of type section
  uint32_t StrOff;   // Offset to string section
  uint32_t StrLen;   // Length of string section
  uint32_t FuncOff;  // Offset to function section
  uint32_t FuncLen;  // Length of function section
};

/// Common type entry header
struct CommonType {
  uint32_t NameOff;    // String table offset (0 for anonymous)
  uint32_t Info;       // Bits 0-15: vlen, Bits 24-28: kind
  uint32_t SizeOrType; // Size (for INT/STRUCT) or TypeID (for PTR/FUNC)

  void setKind(uint8_t Kind) { Info = (Info & 0x00FFFFFF) | (Kind << 24); }
  void setVlen(uint16_t Vlen) { Info = (Info & 0xFFFF0000) | Vlen; }
};

/// INT type extra data
struct IntExtra {
  uint32_t Encoding; // Bits 0-7: bit width, Bit 8: signed
};

/// Array extra data
struct ArrayExtra {
  uint32_t ElemTypeId;
  uint32_t IndexTypeId; // Usually uint32 type
  uint32_t NumElems;
};

/// Struct member
struct Member {
  uint32_t NameOff;
  uint32_t TypeId;
  uint32_t Offset; // Byte offset in struct
};

/// Function parameter
struct Param {
  uint32_t NameOff;
  uint32_t TypeId;
};

/// Function info entry
struct FuncInfo {
  uint32_t NameOff;   // Function name in string table
  uint32_t TypeId;    // References a FUNC type
  uint32_t SymbolIdx; // Symbol table index (for runtime linking)
};

} // namespace ParasolMeta

/// String table with deduplication for the .parasol_meta section.
/// Strings are stored with null terminators. The first entry is always
/// an empty string at offset 0.
class ParasolStringTable {
  uint32_t Size = 0;
  StringMap<uint32_t> StringToOffset;
  std::vector<std::string> Strings;

public:
  ParasolStringTable() {
    // First entry is always empty string
    addString("");
  }

  /// Returns the total size of all strings including null terminators.
  uint32_t getSize() const { return Size; }

  /// Returns the ordered list of strings for emission.
  const std::vector<std::string> &getStrings() const { return Strings; }

  /// Add a string to the table and return its byte offset.
  /// Duplicate strings are deduplicated and return the existing offset.
  uint32_t addString(StringRef S);
};

class ParasolTypeMetadata;

/// Base class for type entries
class ParasolTypeEntry {
protected:
  uint8_t Kind;
  uint32_t Id = 0;
  ParasolMeta::CommonType TypeData = {};

public:
  explicit ParasolTypeEntry(uint8_t Kind) : Kind(Kind) {
    TypeData.setKind(Kind);
  }
  virtual ~ParasolTypeEntry() = default;

  void setId(uint32_t NewId) { Id = NewId; }
  uint32_t getId() const { return Id; }
  uint8_t getKind() const { return Kind; }

  /// Set the SizeOrType field (used for overriding pointee type IDs)
  void setSizeOrType(uint32_t Value) { TypeData.SizeOrType = Value; }

  /// Get total size of this type entry including extra data
  virtual uint32_t getSize() const { return ParasolMeta::CommonTypeSize; }

  /// Complete type after all types have been visited (resolve cross-refs)
  virtual void completeType(ParasolTypeMetadata &Meta) {}

  /// Emit this type entry to the streamer
  virtual void emitType(MCStreamer &OS) const;
};

/// Void type (used for void return type)
class ParasolTypeVoid : public ParasolTypeEntry {
public:
  ParasolTypeVoid() : ParasolTypeEntry(ParasolMeta::KIND_VOID) {}
};

/// Integer type (uint8_t, int32_t, etc.)
class ParasolTypeInt : public ParasolTypeEntry {
  uint32_t IntEncoding = 0;

public:
  ParasolTypeInt(uint32_t BitWidth, bool IsSigned, StringRef Name,
                 ParasolStringTable &StrTab);

  uint32_t getSize() const override {
    return ParasolMeta::CommonTypeSize + sizeof(uint32_t);
  }
  void emitType(MCStreamer &OS) const override;
};

/// Pointer type
class ParasolTypePtr : public ParasolTypeEntry {
  const Type *PointeeTy;
  uint32_t PointeeTypeId = 0;
  bool HasPrecomputedPointeeType = false;

public:
  explicit ParasolTypePtr(const Type *Pointee)
      : ParasolTypeEntry(ParasolMeta::KIND_PTR), PointeeTy(Pointee) {}

  /// Constructor with pre-computed pointee type ID from debug info
  ParasolTypePtr(const Type *Pointee, uint32_t PointeeId)
      : ParasolTypeEntry(ParasolMeta::KIND_PTR), PointeeTy(Pointee),
        PointeeTypeId(PointeeId), HasPrecomputedPointeeType(true) {
    // Set the pointee type ID in SizeOrType immediately
    TypeData.SizeOrType = PointeeId;
  }

  void completeType(ParasolTypeMetadata &Meta) override;
  void emitType(MCStreamer &OS) const override;
};

/// Array type
class ParasolTypeArray : public ParasolTypeEntry {
  const Type *ElemTy;
  uint64_t NumElems;
  uint32_t ElemTypeId = 0;
  uint32_t IndexTypeId = 0;

public:
  ParasolTypeArray(const Type *Elem, uint64_t N)
      : ParasolTypeEntry(ParasolMeta::KIND_ARRAY), ElemTy(Elem), NumElems(N) {}

  uint32_t getSize() const override {
    return ParasolMeta::CommonTypeSize + ParasolMeta::ArrayExtraSize;
  }
  void completeType(ParasolTypeMetadata &Meta) override;
  void emitType(MCStreamer &OS) const override;
};

/// Struct type
class ParasolTypeStruct : public ParasolTypeEntry {
  const StructType *STy;
  std::vector<ParasolMeta::Member> Members;

public:
  ParasolTypeStruct(const StructType *S, StringRef Name,
                    ParasolStringTable &StrTab);

  uint32_t getSize() const override {
    return ParasolMeta::CommonTypeSize +
           Members.size() * ParasolMeta::MemberSize;
  }
  void completeType(ParasolTypeMetadata &Meta) override;
  void emitType(MCStreamer &OS) const override;
};

/// Function prototype type (return type + parameters)
class ParasolTypeFuncProto : public ParasolTypeEntry {
  const FunctionType *FTy;
  std::vector<ParasolMeta::Param> Params;
  uint32_t ReturnTypeId = 0;

  /// Pre-computed parameter type IDs from debug info (if available)
  /// If non-empty, completeType() will use these instead of looking up by LLVM
  /// Type
  std::vector<uint32_t> PrecomputedParamTypes;

  /// Initialize common fields and parameter entries.
  /// \param ParamTypeIds Optional pre-computed type IDs; if empty, TypeIds
  ///        are set to 0 and will be resolved in completeType().
  void initializeParams(const Function *Func, ParasolStringTable &StrTab,
                        ArrayRef<uint32_t> ParamTypeIds);

public:
  ParasolTypeFuncProto(const FunctionType *F, const Function *Func,
                       ParasolStringTable &StrTab);

  /// Constructor with pre-computed type IDs from debug info
  ParasolTypeFuncProto(const FunctionType *F, const Function *Func,
                       ParasolStringTable &StrTab, uint32_t RetTypeId,
                       ArrayRef<uint32_t> ParamTypeIds);

  uint32_t getSize() const override {
    // Return type is in SizeOrType, so only CommonType + params
    return ParasolMeta::CommonTypeSize + Params.size() * ParasolMeta::ParamSize;
  }
  void completeType(ParasolTypeMetadata &Meta) override;
  void emitType(MCStreamer &OS) const override;
};

/// Function type (name + reference to func proto)
class ParasolTypeFunc : public ParasolTypeEntry {
  uint32_t ProtoTypeId;

public:
  ParasolTypeFunc(StringRef Name, uint32_t ProtoId, ParasolStringTable &StrTab);
  void emitType(MCStreamer &OS) const override;
};

/// Function info for an fhe_program function
struct ParasolFuncInfo {
  const MCSymbol *Symbol;
  uint32_t NameOff; // String table offset (already added during processModule)
  uint32_t TypeId;
};

/// Collects and emits type metadata for fhe_program functions.
///
/// This class processes a module to find functions marked with the
/// [[clang::fhe_program]] attribute, extracts their type information
/// (preferring debug info when available for signedness and pointer types),
/// and emits the metadata to the .parasol_meta ELF section.
///
/// Usage:
/// \code
///   ParasolTypeMetadata Meta(AsmPrinter);
///   Meta.processModule(M);  // Call during doInitialization
///   Meta.endModule();       // Call during doFinalization
/// \endcode
class ParasolTypeMetadata {
  std::vector<std::unique_ptr<ParasolTypeEntry>> TypeEntries;

  /// Map from LLVM Type to type ID (for deduplication)
  DenseMap<const Type *, uint32_t> TypeToIdMap;

  /// Map from DIType to type ID (for debug info based types)
  DenseMap<const DIType *, uint32_t> DITypeToIdMap;

  /// Functions marked with fhe_program attribute
  std::vector<ParasolFuncInfo> FheProgramFuncs;

  /// Cached type IDs for common types
  uint32_t VoidTypeId = 0;
  uint32_t Uint32TypeId = 0; // Used for array index type

  /// Add a type entry and return its ID
  uint32_t addType(std::unique_ptr<ParasolTypeEntry> Entry);

  /// Get or create void type
  uint32_t getVoidTypeId();

  /// Emit the .parasol_meta section
  void emitParasolMetaSection();

  ///@{
  /// \name DIType visitor methods
  /// These methods extract types from debug info, preserving signedness and
  /// pointer element types that are lost in LLVM IR.

  /// Dispatch to the appropriate visitor based on DIType kind.
  /// \param DTy Debug info type to visit (may be null for void).
  /// \param Ctx LLVM context for creating void types.
  /// \returns Type ID for the visited type, creating a new entry if needed.
  uint32_t visitTypeEntry(const DIType *DTy, LLVMContext &Ctx);

  /// Visit a basic type (integer, bool, float).
  /// \param BTy Basic type with encoding information for signedness.
  /// \returns Type ID for the integer type with correct signedness.
  uint32_t visitBasicType(const DIBasicType *BTy);

  /// Visit a derived type (pointer, typedef, const, volatile).
  /// \param DTy Derived type to visit.
  /// \param Ctx LLVM context for creating void types.
  /// \returns Type ID, unwrapping qualifiers to the underlying type.
  uint32_t visitDerivedType(const DIDerivedType *DTy, LLVMContext &Ctx);

  /// Visit a composite type (struct, union, array).
  /// \param CTy Composite type to visit.
  /// \returns Type ID (currently falls back to void for structs/arrays).
  uint32_t visitCompositeType(const DICompositeType *CTy);

  /// Visit a subroutine type to create a function prototype.
  /// \param STy Subroutine type with return and parameter types.
  /// \param F Function for parameter names (may be null).
  /// \returns Type ID for the function prototype.
  uint32_t visitSubroutineType(const DISubroutineType *STy, const Function *F);
  ///@}

  /// Check if a DIType is already in the cache.
  /// \returns The cached type ID if found, std::nullopt otherwise.
  std::optional<uint32_t> getCachedDIType(const DIType *Ty) const {
    auto It = DITypeToIdMap.find(Ty);
    if (It != DITypeToIdMap.end())
      return It->second;
    return std::nullopt;
  }

  /// Record an fhe_program function with the given prototype type ID.
  /// Creates a FUNC type entry and adds to the function list.
  /// \param F The function to record.
  /// \param ProtoId Type ID of the function prototype.
  void recordFheProgramFunction(const Function &F, uint32_t ProtoId);

public:
  /// The AsmPrinter used for emitting the metadata section.
  AsmPrinter *const Asm;

  /// String table for type and function names.
  ParasolStringTable StringTable;

  explicit ParasolTypeMetadata(AsmPrinter *AP) : Asm(AP) {}

  /// Scan the module for fhe_program functions and collect their types.
  /// Should be called during AsmPrinter::doInitialization().
  void processModule(const Module &M);

  /// Get or create a type ID for an LLVM type.
  /// Uses the LLVM type cache; does not preserve signedness or pointer types.
  /// Prefer debug info path (via processModule) when available.
  uint32_t getOrCreateTypeId(const Type *Ty);

  /// Returns the type ID for uint32_t, creating it if needed.
  /// Used internally for array index types.
  uint32_t getUint32TypeId();

  /// Returns the string table for adding type and parameter names.
  ParasolStringTable &getStringTable() { return StringTable; }

  /// Complete type resolution and emit the .parasol_meta section.
  /// Should be called during AsmPrinter::doFinalization().
  void endModule();

  /// Returns true if any fhe_program functions were found in the module.
  bool hasFheProgramFunctions() const { return !FheProgramFuncs.empty(); }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_PARASOL_PARASOLTYPEMETADATA_H
