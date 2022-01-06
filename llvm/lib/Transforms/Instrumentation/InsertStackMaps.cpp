#include <map>
#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/Analysis/LiveValues.h"
#include "llvm/Analysis/PopcornUtil.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "insert-stackmaps"

using namespace llvm;

static cl::opt<bool>
NoLiveVals("no-live-vals",
           cl::desc("Don't add live values to inserted stackmaps"),
           cl::init(false),
           cl::Hidden);

namespace {

/* Track slots for unnamed values */
static ModuleSlotTracker *SlotTracker = nullptr;

/* Sort values based on name */
struct ValueComp
{
  bool operator ()(const Value *a, const Value *b)
  {
    if(a->hasName() && b->hasName())
      return a->getName().compare(b->getName()) < 0;
    else if(a->hasName()) return true;
    else if(b->hasName()) return false;
    else {
      int slot_a = SlotTracker->getLocalSlot(a),
          slot_b = SlotTracker->getLocalSlot(b);
      return slot_a < slot_b;
    }
  }
};

/**
 * This class instruments equivalence points in the IR with LLVM's stackmap
 * intrinsic.  This tells the backend to record the locations of IR values
 * after register allocation in a separate ELF section.
 */
class InsertStackMaps : public ModulePass
{
private:
  /* Some useful typedefs */
  typedef SmallVector<const Instruction *, 4> InstVec;
  typedef DenseMap<const Instruction *, InstVec> InstHidingMap;
  typedef SmallVector<const Argument *, 4> ArgVec;
  typedef DenseMap<const Instruction *, ArgVec> ArgHidingMap;

public:
  static char ID;
  size_t callSiteID;
  size_t numInstrumented;

  InsertStackMaps() : ModulePass(ID), callSiteID(0), numInstrumented(0) {
    initializeInsertStackMapsPass(*PassRegistry::getPassRegistry());
  }
  ~InsertStackMaps() {}

  /* ModulePass virtual methods */
  virtual StringRef getPassName() const { return "Insert stackmaps"; }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const
  {
    AU.addRequired<LiveValues>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
  }

  /**
   * Use liveness analysis to insert stackmap intrinsics into the IR to record
   * live values at equivalence points.
   *
   * Note: currently we only insert stackmaps at function call sites.
   */
  virtual bool runOnModule(Module &M)
  {
    bool modified = false;

    std::set<const Value *> *live;
    std::set<const Value *, ValueComp> sortedLive;
    InstHidingMap hiddenInst;
    ArgHidingMap hiddenArgs;

    LLVM_DEBUG(errs() << "\n********** Begin InsertStackMaps **********\n"
                 << "********** Module: " << M.getName() << " **********\n\n");

    this->createSMType(M);
    if(this->addSMDeclaration(M)) modified = true;
    SlotTracker = new ModuleSlotTracker(&M);

    modified |= this->removeOldStackmaps(M);

    /* Iterate over all functions/basic blocks/instructions. */
    for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++)
    {
      if(f->isDeclaration()) continue;

      LLVM_DEBUG(errs() << "InsertStackMaps: entering function "
                   << f->getName() << "\n");

      LiveValues &liveVals = getAnalysis<LiveValues>(*f);
      DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*f).getDomTree();
      SlotTracker->incorporateFunction(*f);
      std::set<const Value *>::const_iterator v, ve;
      getHiddenVals(*f, hiddenInst, hiddenArgs);

      /* Find call sites in the function. */
      for(Function::iterator b = f->begin(), be = f->end(); b != be; b++)
      {
        LLVM_DEBUG(
          errs() << "InsertStackMaps: entering basic block ";
          b->printAsOperand(errs(), false);
          errs() << "\n"
        );

        for(BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; i++)
        {
          if(Popcorn::isCallSite(&*i))
          {
            CallSite CS(&*i);
            if(CS.isInvoke())
            {
              LLVM_DEBUG(dbgs() << "WARNING: unhandled invoke:"; CS->dump());
              continue;
            }

            IRBuilder<> builder(CS->getNextNode());
            std::vector<Value *> args(2);
            args[0] = ConstantInt::getSigned(Type::getInt64Ty(M.getContext()),
                                             this->callSiteID++);
            args[1] = ConstantInt::getSigned(Type::getInt32Ty(M.getContext()),
                                             0);

            if(NoLiveVals) {
              builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
              this->numInstrumented++;
              continue;
            }

            live = liveVals.getLiveValues(&*i);
            for(const Value *val : *live) sortedLive.insert(val);
            for(const auto &pair : hiddenInst) {
              /*
               * The two criteria for inclusion of a hidden value are:
               *   1. The value's definition dominates the call
               *   2. A use which hides the definition is in the stackmap
               */
              if(DT.dominates(pair.first, &*i) && live->count(pair.first))
                for(auto &inst : pair.second) sortedLive.insert(inst);
            }
            for(const auto &pair : hiddenArgs) {
              /*
               * Similar criteria apply as above, except we know arguments
               * dominate the entire function.
               */
              if(live->count(pair.first))
                for(auto &inst : pair.second) sortedLive.insert(inst);
            }
            delete live;

            LLVM_DEBUG(
              const Function *calledFunc;

              errs() << "  ";
              if(!CS->getType()->isVoidTy()) {
                CS->printAsOperand(errs(), false);
                errs() << " ";
              }
              else errs() << "(void) ";

              calledFunc = CS.getCalledFunction();
              if(calledFunc && calledFunc->hasName())
              {
                StringRef name = CS.getCalledFunction()->getName();
                errs() << name << " ";
              }
              errs() << "ID: " << this->callSiteID;

              errs() << ", " << sortedLive.size() << " live value(s)\n   ";
              for(const Value *val : sortedLive) {
                errs() << " ";
                val->printAsOperand(errs(), false);
              }
              errs() << "\n";
            );

            for(v = sortedLive.begin(), ve = sortedLive.end(); v != ve; v++)
              args.push_back(const_cast<Value*>(*v));
            builder.CreateCall(this->SMFunc, ArrayRef<Value*>(args));
            sortedLive.clear();
            this->numInstrumented++;
          }
        }
      }

      hiddenInst.clear();
      hiddenArgs.clear();
      this->callSiteID = 0;
    }

    LLVM_DEBUG(
      errs() << "InsertStackMaps: finished module " << M.getName() << ", added "
             << this->numInstrumented << " stackmaps\n\n";
    );

    if(numInstrumented > 0) modified = true;
    delete SlotTracker;

    return modified;
  }

private:
  /* Name of stack map intrinsic */
  static const StringRef SMName;

  /* Stack map instruction creation */
  Function *SMFunc;
  FunctionType *SMTy; // Used for creating function declaration

  /**
   * Create the function type for the stack map intrinsic.
   */
  void createSMType(const Module &M)
  {
    std::vector<Type*> params(2);
    params[0] = Type::getInt64Ty(M.getContext());
    params[1] = Type::getInt32Ty(M.getContext());
    this->SMTy = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                   ArrayRef<Type*>(params),
                                                   true);
  }

  /**
   * Add the stackmap intrinisic's function declaration if not already present.
   * Return true if the declaration was added, or false if it's already there.
   */
  bool addSMDeclaration(Module &M)
  {
    if(!(this->SMFunc = M.getFunction(this->SMName)))
    {
      LLVM_DEBUG(errs() << "Adding stackmap function declaration to " << M.getName() << "\n");
      this->SMFunc = Function::Create(this->SMTy, Function::ExternalLinkage, this->SMName, M);
      this->SMFunc->setCallingConv(CallingConv::C);
      return true;
    }
    else return false;
  }

  /**
   * Iterate over all instructions, removing previously found stackmaps.
   */
  bool removeOldStackmaps(Module &M)
  {
    bool modified = false;
    CallInst* CI;
    const Function *F;

    LLVM_DEBUG(dbgs() << "Searching for/removing old stackmaps\n";);

    for(Module::iterator f = M.begin(), fe = M.end(); f != fe; f++) {
      for(Function::iterator bb = f->begin(), bbe = f->end(); bb != bbe; bb++) {
        for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; i++) {
          if((CI = dyn_cast<CallInst>(&*i))) {
            F = CI->getCalledFunction();
            if(F && F->hasName() && F->getName() == SMName) {
	      // FIXME: 'i' needs to point to the previous node.
	      i = i->eraseFromParent();
	      i = bb->begin();
              modified = true;
            }
          }
        }
      }
    }

    LLVM_DEBUG(if(modified)
            dbgs() << "WARNING: found previous run of Popcorn passes!\n";);

    return modified;
  }

  /**
   * Gather a list of values which may be "hidden" from live value analysis.
   * This function collects the values used in these instructions, which are
   * later added to the appropriate stackmaps.
   *
   *  - Instructions which access fields of structs or entries of arrays, like
   *    getelementptr, can interfere with the live value analysis to hide the
   *    backing values used in the instruction.  For example, the following IR
   *    obscures %arr from the live value analysis:
   *
   *  %arr = alloca [4 x double], align 8
   *  %arrayidx = getelementptr inbounds [4 x double], [4 x double]* %arr, i64 0, i64 0
   *
   *  -> Access to %arr might only happen through %arrayidx, and %arr may not
   *     be used any more
   *
   */
  void getHiddenVals(Function &F, InstHidingMap &inst, ArgHidingMap &args)
  {
    /* Does the instruction potentially hide values from liveness analysis? */
    auto hidesValues = [](const Instruction *I) {
      if(isa<ExtractElementInst>(I) || isa<InsertElementInst>(I) ||
         isa<ExtractValueInst>(I) || isa<InsertValueInst>(I) ||
         isa<GetElementPtrInst>(I) || isa<BitCastInst>(I))
        return true;
      else return false;
    };

    /* Search for instructions that obscure live values & record operands */
    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
      InstVec &InstsHidden = inst[&*i];
      ArgVec &ArgsHidden = args[&*i];

      if(hidesValues(&*i)) {
        for(unsigned op = 0; op < i->getNumOperands(); op++) {
          if(isa<Instruction>(i->getOperand(op)))
            InstsHidden.push_back(cast<Instruction>(i->getOperand(op)));
          else if(isa<Argument>(i->getOperand(op)))
            ArgsHidden.push_back(cast<Argument>(i->getOperand(op)));
        }
      }
    }
  }
};

} /* end anonymous namespace */

char InsertStackMaps::ID = 0;
const StringRef InsertStackMaps::SMName = "llvm.experimental.pcn.stackmap";

INITIALIZE_PASS_BEGIN(InsertStackMaps, "insert-stackmaps",
                      "Instrument equivalence points with stack maps",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(LiveValues)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(InsertStackMaps, "insert-stackmaps",
                    "Instrument equivalence points with stack maps",
                    false, false)

namespace llvm {
  ModulePass *createInsertStackMapsPass() { return new InsertStackMaps(); }
}
