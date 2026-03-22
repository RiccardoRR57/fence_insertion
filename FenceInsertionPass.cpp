#include "llvm/IR/Function.h"
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

        outs() << "Running on function: " << F.getName() << '\n';
        return PreservedAnalyses::all();
    }
};
} // end anonymous namespace

// Plugin Registration
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