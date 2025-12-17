//===----------------------------------------------------------------------===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/FheProgramFixup.h"

using namespace llvm;

PreservedAnalyses FheProgramFixupPass::run(Function &F,
                                           FunctionAnalysisManager &M) {
  if (!F.hasFnAttribute(Attribute::FheProgram))
    return PreservedAnalyses::all();

  return PreservedAnalyses::none();
}
