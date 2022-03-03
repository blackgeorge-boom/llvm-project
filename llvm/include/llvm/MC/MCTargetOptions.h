//===- MCTargetOptions.h - MC Target Options --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCTARGETOPTIONS_H
#define LLVM_MC_MCTARGETOPTIONS_H

#include <string>
#include <vector>

namespace llvm {

enum class ExceptionHandling {
  None,     ///< No exception support
  DwarfCFI, ///< DWARF-like instruction based exceptions
  SjLj,     ///< setjmp/longjmp based exceptions
  ARM,      ///< ARM EHABI
  WinEH,    ///< Windows Exception Handling
  Wasm,     ///< WebAssembly Exception Handling
};

enum class DebugCompressionType {
  None, ///< No compression
  GNU,  ///< zlib-gnu style compression
  Z,    ///< zlib style complession
};

class StringRef;

class MCTargetOptions {
public:
  enum AsmInstrumentation {
    AsmInstrumentationNone,
    AsmInstrumentationAddress
  };

  bool MCRelaxAll : 1;
  bool MCNoExecStack : 1;
  bool MCFatalWarnings : 1;
  bool MCNoWarn : 1;
  bool MCNoDeprecatedWarn : 1;
  bool MCSaveTempLabels : 1;
  bool MCUseDwarfDirectory : 1;
  bool MCIncrementalLinkerCompatible : 1;
  bool MCPIECopyRelocations : 1;
  bool ShowMCEncoding : 1;
  bool ShowMCInst : 1;
  bool AsmVerbose : 1;

  /// Preserve Comments in Assembly.
  bool PreserveAsmComments : 1;

  int DwarfVersion = 0;

  std::string ABIName;
  std::string SplitDwarfFile;

  /// JSON file describing the callsites padding for each architecture.
  /// The file is in the form:
  /// {
  ///     "x86-64": {
  ///         ".Lmain0": 2,
  ///         ".Lmain1": 1
  ///      },
  ///      "aarch64": {
  ///          ".Lmain0": 12,
  ///          ".Lmain1": 0
  ///      }
  /// }
  /// For example in x86-64 .Lmain0 is a temporary label for the first call inside main.
  /// The number 2 indicates 2 bytes needed for padding.
  /// In aarch64 the number of padding must be a multiple of 4.
  std::string CallsitePaddingFilename;

  /// Disable alignment at the beginning of basic blocks.
  /// When aligning the callsites, we want the second compilation to be
  /// identical with the first, with the only difference of callsite padding.
  /// If we allow alignment at the beginning of basic blocks,
  /// sometimes the second compilation includes additional alignment of blocks,
  /// on top of callsite padding, which ruins the callsite alignment.
  bool DisableBlockAlign;

  /// X86 uses a heuristic to order the symbols in the local stack.
  /// AArch64 does not follow a similar strategy, so disable this ordering
  /// to keep the same stack layout.
  bool DisableX86FrameObjOrder;

  /// Additional paths to search for `.include` directives when using the
  /// integrated assembler.
  std::vector<std::string> IASSearchPaths;

  MCTargetOptions();

  /// getABIName - If this returns a non-empty string this represents the
  /// textual name of the ABI that we want the backend to use, e.g. o32, or
  /// aapcs-linux.
  StringRef getABIName() const;
};

} // end namespace llvm

#endif // LLVM_MC_MCTARGETOPTIONS_H
