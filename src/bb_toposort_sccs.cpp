//------------------------------------------------------------------------------
// bb_toposort_sccs LLVM sample. Demonstrates:
//
//
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//------------------------------------------------------------------------------
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;


class TopoSorter {
public:
  void runToposort(const Function &F) {
    outs() << "Topological sort of function " << F.getName() << ":\n";
    // Initialize the color map by marking all the vertices white.
    for (Function::const_iterator I = F.begin(), IE = F.end(); I != IE; ++I) {
      ColorMap[I] = TopoSorter::WHITE;
    }

    // The BB graph has a single entry vertex from which the other BBs should
    // be discoverable - the function entry block.
    bool success = recursiveDFSToposort(&F.getEntryBlock());
    if (!success) {
      outs() << "  Sorting failed\n";
      return;
    }

    // Now we have all the BBs inside SortedBBs in reverse topological order.
    for (BBVector::const_reverse_iterator RI = SortedBBs.rbegin(),
                                          RE = SortedBBs.rend();
                                          RI != RE; ++RI) {
      outs() << "  " << (*RI)->getName() << "\n";
    }
  }
private:
  enum Color {WHITE, GREY, BLACK};
  typedef DenseMap<const BasicBlock *, Color> BBColorMap;
  typedef SmallVector<const BasicBlock *, 32> BBVector;
  BBColorMap ColorMap;
  BBVector SortedBBs;

  bool recursiveDFSToposort(const BasicBlock *BB) {
    ColorMap[BB] = TopoSorter::GREY;
    const TerminatorInst *TInst = BB->getTerminator();
    for (unsigned I = 0, NSucc = TInst->getNumSuccessors(); I < NSucc; ++I) {
      BasicBlock *Succ = TInst->getSuccessor(I);
      Color SuccColor = ColorMap[Succ];
      if (SuccColor == TopoSorter::WHITE) {
        bool success = recursiveDFSToposort(Succ);
        if (!success)
          return false;
      } else if (SuccColor == TopoSorter::GREY) {
        outs() << "  Detected cycle: edge from " << BB->getName() << 
                  " to " << Succ->getName() << "\n";
        return false;
      }
    }
    // This BB is finished (fully explored), so we can add it to the vector.
    ColorMap[BB] = TopoSorter::BLACK;
    SortedBBs.push_back(BB);
    return true;
  }
};


class AnalyzeBBGraph : public FunctionPass {
public:
  AnalyzeBBGraph() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
    TopoSorter TS;
    TS.runToposort(F);
    return false;
  }

  // The address of this member is used to uniquely identify the class. This is
  // used by the LLVM's own RTTI mechanism.
  static char ID;
};

char AnalyzeBBGraph::ID = 0;

int main(int argc, char **argv) {
  if (argc < 2) {
    errs() << "Usage: " << argv[0] << " <IR file>\n";
    return 1;
  }

  // Parse the input LLVM IR file into a module.
  SMDiagnostic Err;
  Module *Mod = ParseIRFile(argv[1], Err, getGlobalContext());
  if (!Mod) {
    Err.print(argv[0], errs());
    return 1;
  }

  // Create a pass manager and fill it with the passes we want to run.
  PassManager PM;
  PM.add(new AnalyzeBBGraph());
  PM.run(*Mod);

  return 0;
}