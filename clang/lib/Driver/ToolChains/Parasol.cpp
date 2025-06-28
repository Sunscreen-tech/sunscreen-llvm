//===--- Parasol.cpp - Parasol ToolChain Implementations ------------*- C++ -*-===//
//
// Modified by Sunscreen under the AGPLv3 license; see the README at the
// repository root for more information
//
//===----------------------------------------------------------------------===//

#include "Parasol.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

ParasolToolChain::ParasolToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : ToolChain(D, Triple, Args) {
  // ProgramPaths are found via 'PATH' environment variable.
}

bool ParasolToolChain::isPICDefault() const { return true; }

bool ParasolToolChain::isPIEDefault(const ArgList &Args) const { return false; }

bool ParasolToolChain::isPICDefaultForced() const { return true; }

bool ParasolToolChain::SupportsProfiling() const { return false; }

bool ParasolToolChain::hasBlocksRuntime() const { return false; }

Tool *ParasolToolChain::buildLinker() const { return new ParasolLinker(*this); }

bool ParasolLinker::hasIntegratedCPP() const { return false; }

bool ParasolLinker::isLinkJob() const { return true; }

void ParasolLinker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output, const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  assert(Output.isFilename() && "Output must be a file");
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  Args.AddAllArgValues(CmdArgs, options::OPT_Wa_COMMA, options::OPT_Xassembler);

  for (const auto &Input : Inputs) {
    assert(Input.getType() == types::TY_Object && Input.isFilename() && "Input must be an object file");
    CmdArgs.push_back(Input.getFilename());
  }

  std::string Path(getToolChain().getDriver().getInstalledDir());
  const char *Exec = Args.MakeArgString(Path + "/ld.lld");

  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
