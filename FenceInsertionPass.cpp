#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

enum MemoryModel { TSO, PSO };

enum PendingOp : uint8_t {
    None = 0,
    PendingRead = 1 << 0,
    PendingWrite = 1 << 1,
    PendingBoth = PendingRead | PendingWrite
};

struct FenceAnalysisPass : public PassInfoMixin<FenceAnalysisPass> {

    MemoryModel Model;

    FenceAnalysisPass(MemoryModel M) : Model(M) {}

    // This function takes as input the state before the instruction and the intruction itself
    // to calculate the state after, it also sets a flag if a violation is detected
    uint8_t simulateInstruction(Instruction &I, uint8_t CurrentState, bool &ViolationDetected) {
        // If the instruction is a fence we need to clear the state
        if (isa<FenceInst>(&I)) {
            return PendingOp::None;
        }

        // We find a store
        if (isa<StoreInst>(&I)) {
            // TSO forbids store-store anf load-store
            bool tsoViolation = (Model == TSO) && (CurrentState & (PendingOp::PendingWrite | PendingOp::PendingRead));
            // PSO forbids only load-store
            bool psoViolation = (Model == PSO) && (CurrentState & PendingOp::PendingRead);

            // If we have a violation we simulate the insertion of a fence, we clear the state
            if (tsoViolation || psoViolation) {
                ViolationDetected = true;
                CurrentState = PendingOp::None;
            }
            // We found a store so we need to add a pending store to the state
            return CurrentState | PendingOp::PendingWrite;
        }

        // We find a load
        if (isa<LoadInst>(&I)) {
            // In both cases load-load is forbidden and store-load is allowed
            // If we have a violation we simulate the insertion of a fence, we clear the state
            if (CurrentState & PendingOp::PendingRead) {
                ViolationDetected = true;
                CurrentState = PendingOp::None;
            }
            // We found a load so we need to add a pending load to the state
            return CurrentState | PendingOp::PendingRead;
        }

        // We find a RMW or CmpXchg, they act as both load and store
        if (isa<AtomicRMWInst>(&I) || isa<AtomicCmpXchgInst>(&I)) {
            // We have a violation considering instruction as a load
            bool readViolation = (CurrentState & PendingOp::PendingRead);
            // We have a violation considering instruction as a store
            bool writeViolationTSO = (Model == TSO) && (CurrentState & (PendingOp::PendingWrite | PendingOp::PendingRead));
            bool writeViolationPSO = (Model == PSO) && (CurrentState & PendingOp::PendingRead);

            // If we have a violation we simulate the insertion of a fence, we clear the state
            if (readViolation || writeViolationTSO || writeViolationPSO) {
                ViolationDetected = true;
                CurrentState = PendingOp::None;
            }
            // We need to add both pending load and store to the state
            return CurrentState | PendingOp::PendingBoth;
        }

        // If it's not a memory instruction, no change
        return CurrentState;
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        outs() << "Running on function: " << F.getName() << '\n';

        // We perform a dataflow analysis to calculate where we have pending writes or reads
        DenseMap<BasicBlock*, uint8_t> InSets;
        DenseMap<BasicBlock*, uint8_t> OutSets;
        SetVector<BasicBlock*> Worklist;

        // We start by setting every in and aout set as empty and insert every basic block in the worklist
        for (BasicBlock &BB : F) {
            InSets[&BB] = PendingOp::None;
            OutSets[&BB] = PendingOp::None;
            Worklist.insert(&BB);
        }

        // We continue iterating until the worklist is empty
        while (!Worklist.empty()) {
            BasicBlock *BB = Worklist.pop_back_val();

            // We apply union between all the out sets of predecessor blocks to obtain the in set
            uint8_t currentIn = PendingOp::None;
            for (BasicBlock *Pred : predecessors(BB)) {
                currentIn |= OutSets[Pred];
            }
            InSets[BB] = currentIn;

            // We simulate each instruction to calculate the state instruction per instruction
            uint8_t currentOut = currentIn;
            for (Instruction &I : *BB) {
                // In the analysis we don't care if there is an actual violation or not
                bool DummyViolation = false;
                currentOut = simulateInstruction(I, currentOut, DummyViolation);
            }

            // If the out set is changed we need to propagate to successors.
            if (currentOut != OutSets[BB]) {
                OutSets[BB] = currentOut;
                for (BasicBlock *Succ : successors(BB)) {
                    Worklist.insert(Succ);
                }
            }
        }
        
        // Print the results of the analysis
        for (BasicBlock &BB : F) {
            outs() << "  Block: " << BB.getName() << "\n";
            outs() << "    IN:  " << stateToString(InSets[&BB]) << "\n";
            outs() << "    OUT: " << stateToString(OutSets[&BB]) << "\n";
        }

        // Will set this to true when we change something
        bool Changed = false;
        for (BasicBlock &BB : F) {
            // We get the initial state from the analysis
            uint8_t CurrentState = InSets[&BB];
            
            for (auto It = BB.begin(); It != BB.end(); ) {
                Instruction &I = *It++;
                bool ViolationDetected = false;

                // We simulate each instruction to see if we detect a violation
                uint8_t NextState = simulateInstruction(I, CurrentState, ViolationDetected);
                // If we have a violation we insert a fence
                if (ViolationDetected) {
                    outs() << "  -> Inserting SC-Fence before instruction in " << BB.getName() << "\n";
                    IRBuilder<> Builder(&I); 
                    Builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
                    Changed = true;
                }
                // Proceed with the next instruction
                CurrentState = NextState;
            }
        }
        // If we added at least a fence we need to invalidate the analysis
        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

    StringRef stateToString(uint8_t State) {
        if (State == PendingOp::PendingBoth) return "PendingBoth (Read & Write)";
        if (State == PendingOp::PendingWrite) return "PendingWrite";
        if (State == PendingOp::PendingRead) return "PendingRead";
        return "None";
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