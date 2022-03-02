//===- PrefetchAnalysis.h - Prefetching Analysis for Statements ---*- C++ --*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interface for prefetching analysis over structured
// blocks.  The analysis traverses the AST to determine how arrays are accessed
// in structured blocks and generates expressions defining ranges of elements
// accessed inside arrays.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_PREFETCHANALYSIS_H
#define LLVM_CLANG_AST_PREFETCHANALYSIS_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

class LoopNestTraversal;
class ArrayAccessPattern;

namespace clang {

class ASTContext;

/// A range of memory to be prefetched.
class PrefetchRange {
public:
  /// Access type for array.  Sorted in increasing importance.
  enum Type { Read, Write };

  PrefetchRange(enum Type Ty, VarDecl *Array, Expr *Start, Expr *End)
    : Ty(Ty), Array(Array), Start(Start), End(End) {}

  enum Type getType() const { return Ty; }
  VarDecl *getArray() const { return Array; }
  Expr *getStart() const { return Start; }
  Expr *getEnd() const { return End; }
  void setType(enum Type Ty) { this->Ty = Ty; }
  void setArray(VarDecl *Array) { this->Array = Array; }
  void setStart(Expr *Start) { this->Start = Start; }
  void setEnd(Expr *End) { this->End = End; }

  /// Return true if the other prefetch range is equal to this one (ignoring
  /// prefetch type differences), or false otherwise.
  bool equalExceptType(const PrefetchRange &RHS);

  /// Return true if the other prefetch range is equal to this one, or false
  /// otherwise.
  bool operator==(const PrefetchRange &RHS);

  // TODO print & dump
  const char *getTypeName() const {
    if (Ty != Read && Ty != Write)
      return "unknown";
    switch(Ty) {
    case Read: return "read";
    case Write: return "write";
    }
  }

private:
  enum Type Ty;
  VarDecl *Array;
  Expr *Start, *End;
};

class PrefetchAnalysis {
public:
  /// Default constructor, really only defined to enable storage in a DenseMap.
  PrefetchAnalysis() : Ctx(nullptr), S(nullptr) {}

  /// Construct a new prefetch analysis object to analyze a statement.  Doesn't
  /// run the analysis.
  PrefetchAnalysis(ASTContext *Ctx, Stmt *S) : Ctx(Ctx), S(S) {}

  /// Ignore a set of variables during access analysis.  In other words, ignore
  /// memory accesses which use these variables as their base.
  void ignoreVars(const llvm::SmallPtrSet<VarDecl *, 4> &Ignore)
  { this->Ignore = Ignore; }

  /// Analyze the statement to capture loop information & array accesses.
  void analyzeStmt();

  /// Construct prefetch ranges from array accesses & induction variables.
  void calculatePrefetchRanges();

  /// Get prefetch ranges discovered by analysis.
  const SmallVector<PrefetchRange, 8> &getArraysToPrefetch() const
  { return ToPrefetch; }

  /// Return true if the QualType is both scalar and of integer type, or false
  /// otherwise.
  static bool isScalarIntType(const QualType &Ty);

  /// Return the size in bits of a builtin integer type, or UINT32_MAX if not a
  /// builtin integer type.
  static unsigned getTypeSize(BuiltinType::Kind K);

  /// Cast the value declaration to a variable declaration if it is a varaible
  /// of scalar integer type.
  static VarDecl *getVarIfScalarInt(ValueDecl *VD);

  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }

private:
  ASTContext *Ctx;
  Stmt *S;

  /// Analysis information.
  std::shared_ptr<LoopNestTraversal> Loops;
  std::shared_ptr<ArrayAccessPattern> ArrAccesses;

  /// Variables (i.e., arrays) to ignore during analysis
  llvm::SmallPtrSet<VarDecl *, 4> Ignore;

  /// The good stuff -- ranges of memory to prefetch
  llvm::SmallVector<PrefetchRange, 8> ToPrefetch;

  /// Analyze individual types of statements.
  void analyzeForStmt();

  /// Merge overlapping or contiguous prefetch ranges.
  void mergePrefetchRanges();

  /// Remove trivial or redundant array accesses.  This is split into two as
  /// some array accesses may only become redundant after expansion into a
  /// prefetch range.
  void pruneArrayAccesses();
  void prunePrefetchRanges();
};

} // end namespace clang

#endif

