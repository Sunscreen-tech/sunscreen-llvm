//===-- ParasolRegisterInfo.td - Parasol Register defs ---------*- tablegen -*-===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the Parasol register file
//===----------------------------------------------------------------------===//

let Namespace = "Parasol" in {
class ParasolReg<bits<6> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{5-0} = Enc;
  let AltNames = alt;
}

// TODO: Add some registers for the constants zero and one?

// let FallbackRegAltNameIndex = NoRegAltName in
// def ABIRegAltName : RegAltNameIndex;

} // end Namespace

//===----------------------------------------------------------------------===//
//  Integer registers
//===----------------------------------------------------------------------===//
// Define 64 integer registers using a loop
foreach i = 0-63 in {
  def X#i : ParasolReg<i, "x"#i>;
}

// Add to the integer register class
// NOTE: Due to a bug in LLVM this ValueType list needs to be specified in
// largest to smallest order. Otherwise when put into a register bank during
// globalisel the size of the register bank will be too small (it seems to
// choose the first element as the size of the class).
def IR : RegisterClass<"Parasol", [i128, i64, i32, i16, i8, i1], 32, (sequence "X%u", 0, 63)>;
