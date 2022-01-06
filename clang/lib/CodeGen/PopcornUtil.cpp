//===--- PopcornUtil.cpp - LLVM Popcorn Linux Utilities -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <clang/CodeGen/PopcornUtil.h>
#include <llvm/ADT/Triple.h>
#include <llvm/ADT/SmallVector.h>

using namespace clang;
using namespace llvm;

const static std::vector<std::string> PopcornSupported = {
  "aarch64-linux-gnu",
  //"riscv64-linux-gnu",
  "x86_64-linux-gnu"
};

bool Popcorn::SupportedTarget(const StringRef Target) {
  for(auto SupportedTarget : PopcornSupported)
    if(Target == SupportedTarget) return true;
  return false;
}

void Popcorn::GetAllTargets(SmallVector<std::string, 2> &Targets) {
  Targets.clear();
  for(auto Target : PopcornSupported) Targets.push_back(Target);
}

typedef std::shared_ptr<clang::TargetOptions> TargetOptionsPtr;

TargetOptionsPtr Popcorn::GetPopcornTargetOpts(const StringRef TripleStr) {
  Triple Triple(Triple::normalize(TripleStr));
  assert(!Triple.getTriple().empty() && "Invalid target triple");

  TargetOptionsPtr Opts(new TargetOptions);
  Opts->Triple = Triple.getTriple();
  Opts->ABI = "";
  Opts->FPMath = "";
  Opts->FeaturesAsWritten.clear();
  Opts->LinkerVersion = "";

  // TODO need to make CPU selectable & add target features according to CPU

  switch(Triple.getArch()) {
  case Triple::ArchType::aarch64:
    Opts->ABI = "aapcs";
    Opts->CPU = "generic";
    Opts->FeaturesAsWritten.push_back("+neon");
    break;
  case Triple::ArchType::riscv64:
    Opts->ABI = "lp64d";
    Opts->CPU = "";
    Opts->Features.push_back("+m");
    Opts->Features.push_back("+a");
    Opts->Features.push_back("+f");
    Opts->Features.push_back("+d");
    Opts->FeaturesAsWritten.push_back("+m");
    Opts->FeaturesAsWritten.push_back("+a");
    Opts->FeaturesAsWritten.push_back("+f");
    Opts->FeaturesAsWritten.push_back("+d");
    break;
  case Triple::ArchType::x86_64:
    Opts->CPU = "x86-64";
    Opts->FPMath = "sse";
    Opts->FeaturesAsWritten.push_back("+sse");
    Opts->FeaturesAsWritten.push_back("+sse2");
    Opts->FeaturesAsWritten.push_back("+rtm");
    break;
  default: llvm_unreachable("Triple not currently supported on Popcorn");
  }

  return Opts;
}

void Popcorn::StripTargetAttributes(Module &M) {
  /// Target-specific function attributes
  static SmallVector<std::string, 2> TargetAttributes = {
    "target-cpu",
    "target-features"
  };

  for(Function &F : M) {
    AttrBuilder AB(F.getAttributes(), llvm::AttributeList::FunctionIndex);
    for(std::string &Attr : TargetAttributes) {
      if(F.hasFnAttribute(Attr))
        AB.removeAttribute(Attr);
    }
    F.setAttributes(
      AttributeList::get(F.getContext(), AttributeList::FunctionIndex, AB));
  }
}

void Popcorn::AddArchSpecificTargetFeatures(Module &M,
                                            TargetOptionsPtr TargetOpts) {
  static const char *TF = "target-features";
  std::string AllFeatures("");

  for(auto &Feature : TargetOpts->FeaturesAsWritten)
    AllFeatures += Feature + ",";
  AllFeatures = AllFeatures.substr(0, AllFeatures.length() - 1);

  for(Function &F : M) {
    AttrBuilder AB(F.getAttributes(), AttributeList::FunctionIndex);
    assert(!F.hasFnAttribute(TF) && "Target features weren't stripped");
    AB.addAttribute(TF, AllFeatures);
    F.setAttributes(
      AttributeList::get(F.getContext(), AttributeList::FunctionIndex, AB));
  }
}

