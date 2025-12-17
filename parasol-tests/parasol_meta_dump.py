#!/usr/bin/env python3
"""
parasol_meta_dump.py - Parse and display .parasol_meta ELF sections

Usage: python3 parasol_meta_dump.py <elf_file>

Displays function signatures and type information from Parasol ELF files.

Serialization Format
====================

The .parasol_meta section contains type metadata for functions marked with
[[clang::fhe_program]]. All multi-byte values are little-endian.

Section Layout
--------------
    +----------------+
    | Header (32B)   |
    +----------------+
    | Type Section   |
    +----------------+
    | String Section |
    +----------------+
    | Func Section   |
    +----------------+

Header (32 bytes)
-----------------
    Offset  Size  Field       Description
    0       4     Magic       0x41524150 ("PARA" in ASCII, little-endian)
    4       1     Version     Format version (currently 1)
    5       1     Flags       Reserved flags (0)
    6       2     Reserved    Padding (0)
    8       4     TypeOff     Byte offset to type section (from end of header)
    12      4     TypeLen     Length of type section in bytes
    16      4     StrOff      Byte offset to string section
    20      4     StrLen      Length of string section in bytes
    24      4     FuncOff     Byte offset to function section
    28      4     FuncLen     Length of function section in bytes

Type Section
------------
Types are stored sequentially, each starting with a 12-byte common header:

    Offset  Size  Field       Description
    0       4     NameOff     String table offset (0 = anonymous)
    4       4     Info        Bits 0-15: vlen (variable length count)
                              Bits 24-28: kind (type discriminator)
    8       4     SizeOrType  Size in bytes, or referenced type ID

Type IDs are 1-indexed; ID 0 represents void/unknown. Types may have
additional data following the common header depending on kind.

Type Kinds
----------
KIND_VOID (0):
    No extra data. Represents void type.

KIND_INT (1):
    Extra 4 bytes:
        Offset  Size  Field     Description
        0       4     Encoding  Bits 0-7: bit width (8, 16, 32, 64, 128)
                                Bit 8: 1 if signed, 0 if unsigned

    SizeOrType contains the byte size (bit_width / 8).

KIND_PTR (2):
    No extra data. SizeOrType contains the pointee type ID.

KIND_ARRAY (3):
    Extra 12 bytes:
        Offset  Size  Field       Description
        0       4     ElemTypeId  Type ID of array element
        4       4     IndexTypeId Type ID for index (typically uint32)
        8       4     NumElems    Number of elements

KIND_STRUCT (4):
    Variable-length member list. vlen indicates member count.
    Each member is 12 bytes:
        Offset  Size  Field    Description
        0       4     NameOff  Member name (string table offset)
        4       4     TypeId   Member type ID
        8       4     Offset   Byte offset within struct

    SizeOrType contains total struct size in bytes.

KIND_FUNC (5):
    No extra data. Associates a function name with its prototype.
    SizeOrType contains the FUNC_PROTO type ID.

KIND_FUNC_PROTO (6):
    Variable-length parameter list. vlen indicates parameter count.
    SizeOrType contains the return type ID.
    Each parameter is 8 bytes:
        Offset  Size  Field    Description
        0       4     NameOff  Parameter name (string table offset)
        4       4     TypeId   Parameter type ID

String Section
--------------
Null-terminated UTF-8 strings, concatenated. Offset 0 is always an empty
string. Strings are deduplicated; multiple references share the same offset.

Function Section
----------------
List of FHE program functions. Each entry is 12 bytes:
    Offset  Size  Field      Description
    0       4     NameOff    Function name (string table offset)
    4       4     TypeId     Type ID (references a KIND_FUNC entry)
    8       4     SymbolIdx  ELF symbol table index (for linking)

Example
-------
For test_type_metadata.c containing:

    [[clang::fhe_program]]
    uint8_t add_u8(uint8_t a, uint8_t b) { return a + b; }

The metadata would contain:
- String table: "", "uint8_t", "a", "b", "add_u8", ...
- Type 1: INT, name="uint8_t", bits=8, signed=false
- Type 2: FUNC_PROTO, return=1, params=[(name="a", type=1), (name="b", type=1)]
- Type 3: VOID (for void return types)
- Type 4: FUNC, name="add_u8", proto=2
- Function entry: name="add_u8", type=4

Running the dump tool produces:

    $ python3 parasol_meta_dump.py test_type_metadata.o
    FHE Program Functions:
      add_u8(uint8_t a, uint8_t b) -> uint8_t
      ...

    Types:
      [1] INT uint8_t (8 bits, unsigned)
      [2] FUNC_PROTO (uint8_t a, uint8_t b) -> uint8_t
      [3] VOID
      [4] FUNC add_u8 -> proto 2
      ...
"""

import struct
import sys
from pathlib import Path

# Constants matching ParasolTypeMetadata.h
MAGIC = 0x41524150  # "PARA"
HEADER_SIZE = 32
COMMON_TYPE_SIZE = 12

KIND_VOID = 0
KIND_INT = 1
KIND_PTR = 2
KIND_ARRAY = 3
KIND_STRUCT = 4
KIND_FUNC = 5
KIND_FUNC_PROTO = 6

KIND_NAMES = {
    KIND_VOID: "VOID",
    KIND_INT: "INT",
    KIND_PTR: "PTR",
    KIND_ARRAY: "ARRAY",
    KIND_STRUCT: "STRUCT",
    KIND_FUNC: "FUNC",
    KIND_FUNC_PROTO: "FUNC_PROTO",
}


def extract_parasol_meta(elf_path: str) -> bytes | None:
    """Extract .parasol_meta section from ELF file."""
    with open(elf_path, "rb") as f:
        # Read ELF header
        ident = f.read(16)
        if ident[:4] != b"\x7fELF":
            raise ValueError("Not an ELF file")

        is_64bit = ident[4] == 2
        is_le = ident[5] == 1

        endian = "<" if is_le else ">"

        if is_64bit:
            # 64-bit ELF header
            f.seek(40)  # e_shoff
            shoff = struct.unpack(f"{endian}Q", f.read(8))[0]
            f.seek(58)  # e_shentsize, e_shnum, e_shstrndx
            shentsize, shnum, shstrndx = struct.unpack(f"{endian}HHH", f.read(6))
        else:
            # 32-bit ELF header
            f.seek(32)  # e_shoff
            shoff = struct.unpack(f"{endian}I", f.read(4))[0]
            f.seek(46)
            shentsize, shnum, shstrndx = struct.unpack(f"{endian}HHH", f.read(6))

        # Read section header string table
        f.seek(shoff + shstrndx * shentsize)
        if is_64bit:
            f.seek(shoff + shstrndx * shentsize + 24)
            str_offset = struct.unpack(f"{endian}Q", f.read(8))[0]
            str_size = struct.unpack(f"{endian}Q", f.read(8))[0]
        else:
            f.seek(shoff + shstrndx * shentsize + 16)
            str_offset = struct.unpack(f"{endian}I", f.read(4))[0]
            str_size = struct.unpack(f"{endian}I", f.read(4))[0]

        f.seek(str_offset)
        shstrtab = f.read(str_size)

        # Find .parasol_meta section
        for i in range(shnum):
            f.seek(shoff + i * shentsize)
            if is_64bit:
                name_idx = struct.unpack(f"{endian}I", f.read(4))[0]
                f.seek(shoff + i * shentsize + 24)
                sec_offset = struct.unpack(f"{endian}Q", f.read(8))[0]
                sec_size = struct.unpack(f"{endian}Q", f.read(8))[0]
            else:
                name_idx = struct.unpack(f"{endian}I", f.read(4))[0]
                f.seek(shoff + i * shentsize + 16)
                sec_offset = struct.unpack(f"{endian}I", f.read(4))[0]
                sec_size = struct.unpack(f"{endian}I", f.read(4))[0]

            # Get section name
            name_end = shstrtab.find(b"\x00", name_idx)
            name = shstrtab[name_idx:name_end].decode("utf-8")

            if name == ".parasol_meta":
                f.seek(sec_offset)
                return f.read(sec_size)

    return None


class ParasolMetaParser:
    def __init__(self, data: bytes):
        self.data = data
        self.types = {}  # type_id -> type_info
        self.strings = b""
        self.functions = []

    def get_string(self, offset: int) -> str:
        end = self.strings.find(b"\x00", offset)
        return self.strings[offset:end].decode("utf-8")

    def parse(self):
        # Parse header
        magic, version, flags, reserved = struct.unpack_from("<IBBH", self.data, 0)
        if magic != MAGIC:
            raise ValueError(f"Invalid magic: {magic:#x}, expected {MAGIC:#x}")
        if version != 1:
            raise ValueError(f"Unsupported version: {version}")

        type_off, type_len, str_off, str_len, func_off, func_len = struct.unpack_from(
            "<IIIIII", self.data, 8
        )

        # Extract sections (offsets are from after header)
        base = HEADER_SIZE
        type_data = self.data[base + type_off : base + type_off + type_len]
        self.strings = self.data[base + str_off : base + str_off + str_len]
        func_data = self.data[base + func_off : base + func_off + func_len]

        # Parse types
        self._parse_types(type_data)

        # Parse functions
        for i in range(0, len(func_data), 12):
            name_off, type_id, symbol_ref = struct.unpack_from("<III", func_data, i)
            self.functions.append(
                {"name": self.get_string(name_off), "type_id": type_id}
            )

    def _parse_types(self, data: bytes):
        offset = 0
        type_id = 1

        while offset < len(data):
            name_off, info, size_or_type = struct.unpack_from("<III", data, offset)
            kind = (info >> 24) & 0x1F
            vlen = info & 0xFFFF
            offset += COMMON_TYPE_SIZE

            type_info = {
                "kind": kind,
                "name": self.get_string(name_off) if name_off else "",
                "size_or_type": size_or_type,
                "vlen": vlen,
            }

            if kind == KIND_INT:
                encoding = struct.unpack_from("<I", data, offset)[0]
                type_info["bits"] = encoding & 0xFF
                type_info["signed"] = bool(encoding & 0x100)
                offset += 4

            elif kind == KIND_ARRAY:
                elem_id, index_id, num_elems = struct.unpack_from("<III", data, offset)
                type_info["elem_type"] = elem_id
                type_info["index_type"] = index_id
                type_info["num_elems"] = num_elems
                offset += 12

            elif kind == KIND_STRUCT:
                members = []
                for _ in range(vlen):
                    m_name, m_type, m_off = struct.unpack_from("<III", data, offset)
                    members.append(
                        {
                            "name": self.get_string(m_name),
                            "type": m_type,
                            "offset": m_off,
                        }
                    )
                    offset += 12
                type_info["members"] = members

            elif kind == KIND_FUNC_PROTO:
                params = []
                for _ in range(vlen):
                    p_name, p_type = struct.unpack_from("<II", data, offset)
                    params.append({"name": self.get_string(p_name), "type": p_type})
                    offset += 8
                type_info["params"] = params
                type_info["return_type"] = size_or_type

            self.types[type_id] = type_info
            type_id += 1

    def format_type(self, type_id: int, depth: int = 0) -> str:
        if type_id == 0 or type_id not in self.types:
            return "?"
        if depth > 10:
            return "..."

        t = self.types[type_id]
        kind = t["kind"]

        if kind == KIND_VOID:
            return "void"
        elif kind == KIND_INT:
            return t["name"] or f"i{t['bits']}"
        elif kind == KIND_PTR:
            return f"{self.format_type(t['size_or_type'], depth + 1)}*"
        elif kind == KIND_ARRAY:
            return f"{self.format_type(t['elem_type'], depth + 1)}[{t['num_elems']}]"
        elif kind == KIND_STRUCT:
            return f"struct {t['name']}" if t["name"] else "struct {...}"
        elif kind == KIND_FUNC:
            return self.format_type(t["size_or_type"], depth + 1)
        elif kind == KIND_FUNC_PROTO:
            ret = self.format_type(t["return_type"], depth + 1)
            params = ", ".join(
                f"{self.format_type(p['type'], depth + 1)} {p['name']}"
                for p in t.get("params", [])
            )
            return f"({params}) -> {ret}"
        return "?"

    def print_functions(self):
        print("FHE Program Functions:")
        for func in self.functions:
            t = self.types.get(func["type_id"], {})
            if t.get("kind") == KIND_FUNC:
                proto_id = t["size_or_type"]
                sig = self.format_type(proto_id)
                print(f"  {func['name']}{sig}")
            else:
                print(f"  {func['name']}(...)")
        print()

    def print_types(self):
        print("Types:")
        for type_id, t in sorted(self.types.items()):
            kind_name = KIND_NAMES.get(t["kind"], "?")
            name = t.get("name", "")

            if t["kind"] == KIND_INT:
                sign = "signed" if t.get("signed") else "unsigned"
                print(f"  [{type_id}] {kind_name} {name} ({t['bits']} bits, {sign})")
            elif t["kind"] == KIND_STRUCT:
                members = t.get("members", [])
                if members:
                    fields = ", ".join(
                        f"{m['name']}: {self.format_type(m['type'])} @{m['offset']}"
                        for m in members
                    )
                    print(f"  [{type_id}] {kind_name} {name} {{ {fields} }}")
                else:
                    print(f"  [{type_id}] {kind_name} {name} {{}}")
            elif t["kind"] == KIND_ARRAY:
                elem = self.format_type(t["elem_type"])
                print(f"  [{type_id}] {kind_name} {elem}[{t['num_elems']}]")
            elif t["kind"] == KIND_PTR:
                pointee = self.format_type(t["size_or_type"])
                print(f"  [{type_id}] {kind_name} -> {pointee}")
            elif t["kind"] == KIND_FUNC_PROTO:
                sig = self.format_type(type_id)
                print(f"  [{type_id}] {kind_name} {sig}")
            elif t["kind"] == KIND_FUNC:
                print(f"  [{type_id}] {kind_name} {name} -> proto {t['size_or_type']}")
            else:
                print(f"  [{type_id}] {kind_name} {name}")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <elf_file>")
        sys.exit(1)

    elf_path = sys.argv[1]
    if not Path(elf_path).exists():
        print(f"Error: File not found: {elf_path}")
        sys.exit(1)

    data = extract_parasol_meta(elf_path)
    if data is None:
        print(f"No .parasol_meta section found in {elf_path}")
        sys.exit(1)

    print(f"Parasol Metadata from {Path(elf_path).name}")
    print(f"Section size: {len(data)} bytes")
    print()

    parser = ParasolMetaParser(data)
    parser.parse()
    parser.print_functions()
    parser.print_types()


if __name__ == "__main__":
    main()
