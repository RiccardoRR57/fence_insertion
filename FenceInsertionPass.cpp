#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

enum MemoryModel { TSO, PSO };

struct FenceAnalysisPass : public PassInfoMixin<FenceAnalysisPass> {

    MemoryModel Model;
    FenceAnalysisPass(MemoryModel M) : Model(M) {}

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        outs() << "Running FenceAnalysisPass on function: " << F.getName() << '\n';

        bool Modified = false;

        for (auto &BB : F) {
            Instruction *PrevMemOp = nullptr;
            bool PrevWasStore = false;

            // Collect all instructions so as not to modify the the list we're iterating
            SmallVector<Instruction *, 16> Instrs;
            for (auto &I : BB) {
                Instrs.push_back(&I);
            }

            for (auto *I : Instrs) {
                bool CurrIsStore = isa<StoreInst>(I);
                bool CurrIsLoad  = isa<LoadInst>(I);

                // If this instruction is neither a load nor a store, we can skip it
                if (!CurrIsStore && !CurrIsLoad) continue;

                outs() << "  Found " << (CurrIsStore ? "store" : "load")
                       << ": " << *I << '\n';

                if (PrevMemOp) {
                    bool NeedFence = false;

                    if (Model == TSO) {
                        NeedFence = PrevWasStore && CurrIsLoad;
                    } else {
                        NeedFence = PrevWasStore && (CurrIsLoad || CurrIsStore);
                    }

                    if (NeedFence) {
                        // Insert an acquire+release ("seq_cst") fence immediately before the current instruction
                        IRBuilder<> Builder(I);
                        Builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
                        outs() << "    Inserted fence before: " << *I << '\n';
                        Modified = true;
                    }
                }

                PrevMemOp    = I;
                PrevWasStore = CurrIsStore;
            }
        }

        return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};
} // end anonymous namespace

// Plugin Registration (unchanged)
llvm::PassPluginLibraryInfo getFenceAnalysisPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "FenceAnalysisPass", "v0.1",
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "tso") {
                            FPM.addPass(FenceAnalysisPass(TSO));
                            return true;
                        }
                        if (Name == "pso") {
                            FPM.addPass(FenceAnalysisPass(PSO));
                            return true;
                        }
                        return false;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getFenceAnalysisPassPluginInfo();
}