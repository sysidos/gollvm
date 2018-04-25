//===-- LinuxToolChain.cpp ------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Gollvm driver helper class LinuxToolChain methods.
//
//===----------------------------------------------------------------------===//

#include "LinuxToolChain.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "CompileGo.h"
#include "Driver.h"
#include "GnuTools.h"
#include "Tool.h"
#include "ToolChain.h"

using namespace gollvm::driver;

namespace toolchains {

static void addIfPathExists(pathlist &paths, const llvm::Twine &path)
{
  if (llvm::sys::fs::exists(path))
    paths.push_back(path.str());
}

static llvm::StringRef getOSLibDir(const llvm::Triple &triple)
{
  // x86 uses the lib32 variant, unlike other archs.
  if (triple.getArch() == llvm::Triple::x86)
    return "lib32";
  return triple.isArch32Bit() ? "lib" : "lib64";
}

Linux::Linux(gollvm::driver::Driver &driver,
             const llvm::Triple &targetTriple)
    : ToolChain(driver, targetTriple),
      inspectFS_(gnutools::gccdetect::InspectRealFS()),
      gccDetector_(targetTriple,
                   driver.sysRoot(),
                   inspectFS_)
{
  gccDetector_.init();

  // Program paths
  pathlist &ppaths = programPaths();
  auto ftrip = gccDetector_.foundTriple().str();
  addIfPathExists(ppaths, llvm::Twine(gccDetector_.getParentLibPath() +
                                      "/../" + ftrip +
                                      "/bin").str());


  // File paths
  pathlist &fpaths = filePaths();
  addIfPathExists(fpaths, gccDetector_.getLibPath());
  const std::string osLibDir = getOSLibDir(targetTriple);
  addIfPathExists(fpaths, llvm::Twine(gccDetector_.getParentLibPath() +
                                      "/../" + ftrip).str());
  addIfPathExists(fpaths, llvm::Twine(osLibDir + ftrip).str());

  // Include program and file paths in verbose output.
  if (driver.args().hasArg(gollvm::options::OPT_v)) {
    llvm::errs() << "Candidate GCC install:\n" << gccDetector_.toString();

    llvm::errs() << "ProgramPaths:\n";
    for (auto &path : programPaths())
      llvm::errs() << path << "\n";
    llvm::errs() << "FilePaths:\n";
    for (auto &path : filePaths())
      llvm::errs() << path << "\n";
  }
}

Linux::~Linux()
{
}

Tool *Linux::buildCompiler()
{
  return new CompileGo(*this);
}

Tool *Linux::buildAssembler()
{
  return new gnutools::Assembler(*this);
}

Tool *Linux::buildLinker()
{
  return new gnutools::Linker(*this);
}

std::string Linux::getDynamicLinker(const llvm::opt::ArgList &args)
{
  const llvm::Triple::ArchType arch = triple().getArch();

  std::string LibDir;
  std::string Loader;

  switch (arch) {
    default:
      assert(false && "unsupported architecture");
      return "<unknown_dynamic_linker>";
    case llvm::Triple::x86:
      LibDir = "lib";
      Loader = "ld-linux.so.2";
      break;
    case llvm::Triple::x86_64: {
      LibDir = "lib64";
      Loader = "ld-linux-x86-64.so.2";
      break;
    }
  }
  return "/" + LibDir + "/" + Loader;
}

} // end namespace toolchains