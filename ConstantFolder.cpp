#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ADT/SmallVector.h"
#include<iostream>
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace {

typedef enum {
    UNDEF,
    NAC,
    CONST
} ValueState;

class ValueContainer{
    public: 
    ValueState State;
    llvm::Constant *ConstantValue;
    Instruction *Inst;
    ValueContainer(Instruction *instr)
    {
        Inst = instr;
        if(!areAllOperandsConstants())
        {
            State = NAC;
        }

        ConstantValue = llvm::ConstantFoldInstruction(Inst, Inst->getModule()->getDataLayout());
        if(!ConstantValue)
        {
            State = NAC;
        }
        else
        {
            State = CONST;
        }
    }

    bool areAllOperandsConstants() {
        for (unsigned i = 0, e = Inst->getNumOperands(); i != e; ++i) { 
            llvm::Value *Op = Inst->getOperand(i);
            if (!llvm::isa<llvm::Constant>(Op)) {
                return false;
            }
        }
        return true;
    }

    void ReplaceUse(llvm::Instruction *userInst)
    {
        for (unsigned j = 0; j < userInst->getNumOperands(); j++) {
            if (userInst->getOperand(j) == Inst) {
                userInst->setOperand(j, ConstantValue);
            }
        }
    }
};

bool isInlineable(CallInst *call) {
    Function *calledFunction = call->getCalledFunction();
    if (!calledFunction || calledFunction->isDeclaration()) {
        return false; 
    }

    // Check if all arguments are constants
    bool allArgsConstants = true;
    for (unsigned i = 0, e = call->getNumOperands(); i != e; ++i) {
        if (!isa<Constant>(call->getOperand(i))) {
            allArgsConstants = false;
            return false;
        }
    }
    return true;
}

bool isConstantBranch(llvm::Instruction *Inst) { //Need to check if any branches have become constant
    if (auto *BI = llvm::dyn_cast<llvm::BranchInst>(Inst)) {
        if (BI->isConditional()) { //Check for a conditional branch
            
            llvm::Value *Cond = BI->getCondition();
            if (auto *ConstCond = llvm::dyn_cast<llvm::Constant>(Cond)) { //Make sure rhe condition is a constant
                if (auto *ConstBool = llvm::dyn_cast<llvm::ConstantInt>(ConstCond)) {
                    if (ConstBool->isOne() || ConstBool->isZero()) {
                        return true;
                    }
                }
            }

        }
    }
    return false;
}

llvm::BranchInst* replaceConstantBranch(llvm::Instruction *Inst) {

    auto *BI = llvm::dyn_cast<llvm::BranchInst>(Inst);
    if (!BI || !isConstantBranch(BI)) //Guard condition
    return NULL;

    llvm::ConstantInt* ConstCond = llvm::dyn_cast<llvm::ConstantInt>(BI->getCondition());

    // Determine the taken branch
    llvm::BasicBlock *Successor = ConstCond->getZExtValue() ? BI->getSuccessor(0) : BI->getSuccessor(1);
    
    // Replace with an unconditional branch
    llvm::BranchInst* NewInstruction = llvm::BranchInst::Create(Successor, BI->getParent());
    BI->eraseFromParent();

    return NewInstruction;
}

int countPredecessors(llvm::BasicBlock *BB) {
    if (!BB) {
        llvm::errs() << "Error: BasicBlock is nullptr\n";
        return 0;
    }

    int count = 0;
    llvm::pred_iterator PI = llvm::pred_begin(BB), E = llvm::pred_end(BB);
    while (PI != E) {
        ++count;
        ++PI;
    }

    return count;
}

bool isPredecessor(llvm::BasicBlock* checkPred, llvm::BasicBlock* Searchblock) 
{
    if (!checkPred || !Searchblock) {//Guard agains any NULL pointer
        llvm::errs()<<"isPredecessor() got NULL ptr warning\n";
        return false;  
    }

    // Iterate over all predecessors of 'Searchblock'
    for (llvm::pred_iterator PI = llvm::pred_begin(Searchblock), E = llvm::pred_end(Searchblock); PI != E; ++PI) {
        llvm::BasicBlock* pred = *PI;
        if (pred == checkPred) {
            return true;  // Found 'checkPred' in the list of predecessors.
        }
    }
    return false;  // 'checkPred' is not a predecessor of 'Searchblock'.
}

llvm::BasicBlock* getFirstPredecessor(llvm::BasicBlock* BB) {
    if (!BB) {
        return nullptr;
    }

    if (pred_begin(BB) == pred_end(BB)) {
        return nullptr;
    }

    // Access the first predecessor
    llvm::BasicBlock* firstPred = *pred_begin(BB);
    return firstPred;
}

void safelyDeleteBB(llvm::BasicBlock* toDelete)
{
    if(!toDelete) //Guard against NULL pointers
        return;
    //First iterate through successors and remove any phi nodes that refernce it
    for (llvm::succ_iterator SI = llvm::succ_begin(toDelete), E = llvm::succ_end(toDelete); SI != E; ++SI) {

        llvm::BasicBlock *Succ = *SI; 
        for (llvm::Instruction &Inst : *Succ) {

            if (llvm::PHINode *Phi = llvm::dyn_cast<llvm::PHINode>(&Inst)) { //Look for phi nodes
                //And  remove the incoming value that comes from the basic block BB
                int Index = Phi->getBasicBlockIndex(toDelete);
                Phi->removeIncomingValue(Index, true); //If the phi node is empty, go ahead and delete it
            }
            
        }
    }
    toDelete->eraseFromParent();
}

bool SafelyMergeBasicBlocks(llvm::BasicBlock* BlockToMerge,llvm::BasicBlock* BlockMergerInto)
{
    //Guard against Nullptr
    if(!BlockToMerge||!BlockMergerInto)
        return false;

    //TODO add a check to ensure an only successor-only predecessor relationship
    llvm::BranchInst *BI = llvm::dyn_cast<llvm::BranchInst>(BlockMergerInto->getTerminator());
    if ((!BI || !BI->isUnconditional() || BI->getSuccessor(0) != BlockToMerge)) {
        llvm::errs() << "Error: Blocks are not suitable for merging.\n";
        return false;
    }
    //First Update any PHI nodes pointing to the block we want to merge for successors of Block To merge
    for (llvm::succ_iterator SI = llvm::succ_begin(BlockToMerge), E = llvm::succ_end(BlockToMerge); SI != E; ++SI) {
        llvm::BasicBlock *Succ = *SI;
        
        for (llvm::Instruction &Inst : *Succ) { // Iterate over each instruction in the successor block      
            if (auto *Phi = llvm::dyn_cast<llvm::PHINode>(&Inst)) { //Check for Phi nodes
                int Index = Phi->getBasicBlockIndex(BlockToMerge);
                while (Index != -1) { 
                    Phi->setIncomingBlock(Index, BlockMergerInto);
                    Index = Phi->getBasicBlockIndex(BlockToMerge);
                }
            }

        }
    }

    llvm::Instruction *Terminator = BlockMergerInto->getTerminator();
    if (!Terminator) {
        llvm::errs() << "Error: No terminator in the target block.\n";
        return false;
    }

    llvm::IRBuilder<> Builder(BlockMergerInto);
    Builder.SetInsertPoint(Terminator); // Set the insert point before the terminator of BB2

    // Move instructions from before BlockMergerInto terminator
    llvm::Instruction *BB1Terminator = BlockToMerge->getTerminator();
    while (!BlockToMerge->empty() && &BlockToMerge->front() != BB1Terminator) {
        llvm::Instruction &Inst = BlockToMerge->front();
        Inst.removeFromParent();
        Builder.Insert(&Inst); // Insert the instruction into BB2 before its terminator
    }
    llvm::Instruction *ClonedTerminator = BB1Terminator->clone();
    Builder.Insert(ClonedTerminator);
    Terminator->eraseFromParent();
    BlockToMerge->eraseFromParent();

    return true;
}

struct ConstantPropogation : public FunctionPass {
  static char ID;
  ConstantPropogation() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    llvm::SmallVector<ValueContainer, 16> WorkList;
    llvm::SmallVector<llvm::CallInst*, 16> InlineableCalls;

    /*Begin by inlining functions to provide maximum oppurtunity to 
    "catch" constants in the worklist further down*/
    for(BasicBlock &BB : F)  
    {
      for(Instruction &inst : BB)
      {
        if (llvm::isa<llvm::CallInst>(inst)) { //We try to find function calls with all values predefined -- INLINE them!!
            llvm::CallInst *Callinst = dyn_cast<llvm::CallInst>(&inst);
            if(isInlineable(Callinst))
            {
                InlineableCalls.push_back(Callinst);
            }
        }
      }
    }

    bool allInlineSuccess = true;

    while(!InlineableCalls.empty()){
        llvm::CallInst *inst = InlineableCalls.pop_back_val();
        Function *calledFn = inst->getCalledFunction();

        ValueToValueMapTy VMap;
        auto argIter = calledFn->arg_begin();
        for (unsigned i = 0; i < inst->getNumOperands() - 1; ++i, ++argIter) {
            VMap[&*argIter] = inst->getOperand(i);
        }

        InlineFunctionInfo IFI;
        if(!InlineFunction(*inst,IFI).isSuccess()){
            allInlineSuccess = false;
        }
    }

    for(BasicBlock &BB : F)  //Iterate over the basic blocks
    {
      for(Instruction &inst : BB) //Iterate over the instructions
      {
        ValueContainer newVc(&inst);
        if(newVc.ConstantValue)
        {
            WorkList.insert(WorkList.begin(),newVc);
        }
      }
    }

    //Worklist for reaching defs of constants
    while (!WorkList.empty()) {
        ValueContainer WorklistItem = WorkList.pop_back_val();
        llvm::Instruction *instr = WorklistItem.Inst;
        llvm::Constant *constant = WorklistItem.ConstantValue;

        llvm::SmallVector<llvm::Instruction*, 8> ValidUseList;

        for (auto &Use : instr->uses()) {
            llvm::User *user = Use.getUser();  // User is anything that uses the instr
            if (llvm::Instruction *userInst = llvm::dyn_cast<llvm::Instruction>(user)) {
                ValidUseList.push_back(userInst);
            }
        }


        while(!ValidUseList.empty())
        {
            llvm::Instruction *userInstr = ValidUseList.pop_back_val();
            WorklistItem.ReplaceUse(userInstr);
            ValueContainer newVc(userInstr);
            if(newVc.State == CONST)
                WorkList.insert(WorkList.begin(),newVc);
        }

        if (instr && !instr->mayHaveSideEffects() && instr->use_empty()) {
            instr->eraseFromParent();
        }
        // errs()<<" \n\n";
    }

    return true;
  }

}; // end of struct Constant Folding


struct ConditionalConstantPropogation : public FunctionPass {
  static char ID;
  ConditionalConstantPropogation() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    llvm::SmallVector<ValueContainer, 16> WorkList;
    llvm::SmallVector<llvm::CallInst*, 16> InlineableCalls;

    /*Begin by inlining functions to provide maximum oppurtunity to 
    "catch" constants in the worklist further down*/
    for(BasicBlock &BB : F)  
    {
      for(Instruction &inst : BB)
      {
        if (llvm::isa<llvm::CallInst>(inst)) { //We try to find function calls with all values predefined -- INLINE them!!
            llvm::CallInst *Callinst = dyn_cast<llvm::CallInst>(&inst);
            if(isInlineable(Callinst))
            {
                InlineableCalls.push_back(Callinst);
            }
        }
      }
    }

    bool allInlineSuccess = true;

    while(!InlineableCalls.empty()){
        llvm::CallInst *inst = InlineableCalls.pop_back_val();
        Function *calledFn = inst->getCalledFunction();

        ValueToValueMapTy VMap;
        auto argIter = calledFn->arg_begin();
        for (unsigned i = 0; i < inst->getNumOperands() - 1; ++i, ++argIter) {
            VMap[&*argIter] = inst->getOperand(i);
        }

        InlineFunctionInfo IFI;
        if(!InlineFunction(*inst,IFI).isSuccess()){
            allInlineSuccess = false;
        }
    }

    for(BasicBlock &BB : F)  //Iterate over the basic blocks
    {
      for(Instruction &inst : BB) //Iterate over the instructions
      {
        ValueContainer newVc(&inst);
        if(newVc.ConstantValue)
        {
            WorkList.insert(WorkList.begin(),newVc);
        }
      }
    }

    //Worklist for reaching defs of constants
    while (!WorkList.empty()) {
        ValueContainer WorklistItem = WorkList.pop_back_val();
        llvm::Instruction *instr = WorklistItem.Inst;
        llvm::Constant *constant = WorklistItem.ConstantValue;

        llvm::SmallVector<llvm::Instruction*, 8> ValidUseList;

        for (auto &Use : instr->uses()) {
            llvm::User *user = Use.getUser();  // User is anything that uses the instr
            if (llvm::Instruction *userInst = llvm::dyn_cast<llvm::Instruction>(user)) {
                ValidUseList.push_back(userInst);
            }
        }


        while(!ValidUseList.empty())
        {
            llvm::Instruction *userInstr = ValidUseList.pop_back_val();
            WorklistItem.ReplaceUse(userInstr);
            ValueContainer newVc(userInstr);

            if(isConstantBranch(userInstr)) //Branches that become predtermined turn up here
            //We will replace them with constant branches, followed by deleting any branches with no refs
            //Also update Phi nodes for all undeleted branches
            {
                llvm::BranchInst *BI = llvm::dyn_cast<llvm::BranchInst>(userInstr);
                llvm::BasicBlock *CurrentBB = BI->getParent();
                llvm::ConstantInt* cond = llvm::dyn_cast<llvm::ConstantInt>(BI->getCondition());
                llvm::BasicBlock* RemovedSuccessor = NULL;
                llvm::BasicBlock* RemainingSuccessor = NULL;

                //Try to get the removed branch false is at place 1 and true is at place 0
                if(cond->isOne())
                {
                    RemovedSuccessor = BI->getSuccessor(1);
                    RemainingSuccessor = BI->getSuccessor(0);
                }
                else if(cond->isZero())
                {
                    RemovedSuccessor = BI->getSuccessor(0);
                    RemainingSuccessor = BI->getSuccessor(1);
                }
                if(RemovedSuccessor && RemainingSuccessor) //Guard in case something else has happened here 
                {
                    //Need to check whether this successor has only one predecessor...this basic Searchblock
                    if(countPredecessors(RemovedSuccessor)==1 && isPredecessor(BI->getParent(),RemovedSuccessor))
                    {
                        //This is now a block we can remove need to remove all ref, then remove it
                        safelyDeleteBB(RemovedSuccessor);
                    }
                }          
                llvm::BranchInst* NewBI = replaceConstantBranch(userInstr);
                if(RemovedSuccessor && RemainingSuccessor) //Guard in case something else has happened here 
                {
                    // Now we check for any blocks which can be 'merged'. We will need to merge the other predecessor of
                    // Removed successor and Removed successor
                    if(countPredecessors(RemainingSuccessor)==1 && isPredecessor(NewBI->getParent(),RemainingSuccessor))
                    {
                        // errs()<<*RemainingSuccessor<<*BI->getParent()<<"\n\n\n\n\n";
                        //SafelyMergeBasicBlocks(RemainingSuccessor,NewBI->getParent());
                        MergeBlockIntoPredecessor(NewBI->getParent());
                    } 
                }      
            }
            if(newVc.State == CONST)
                WorkList.insert(WorkList.begin(),newVc);
        }

        if (instr && !instr->mayHaveSideEffects() && instr->use_empty()) {
            instr->eraseFromParent();
        }
        // errs()<<" \n\n";
    }

    llvm::SmallVector <PHINode*,16> RougePhiNodelist;

    //Remove any rouge phi nodes
    for(BasicBlock &BB : F)  //Iterate over the basic blocks
    {
      for(Instruction &I : BB) //Iterate over the instructions
      {
        llvm::PHINode *Phi = llvm::dyn_cast<llvm::PHINode>(&I);
        if (!Phi) continue;  // Skip if the instruction is not a PHINode

        // Check if the PHINode has only one incoming value
        if (Phi->getNumIncomingValues() == 1) {
            RougePhiNodelist.push_back(Phi);
        }
      }
    }

    while(!RougePhiNodelist.empty())
    {
        PHINode *Phi =  RougePhiNodelist.pop_back_val();            
        llvm::Value *SingleValue = Phi->getIncomingValue(0);
        Phi->replaceAllUsesWith(SingleValue); //Replace the value
        Phi->eraseFromParent();
        break;
    }

    //Final try to simplify the CFG, try to merge any existing blocks with their parents
    bool MergeOccured;
    do{
        MergeOccured = false;
        std::vector<BasicBlock*> blocks;
        blocks.reserve(std::distance(F.begin(), F.end()));
        for (BasicBlock &BB : F) {
            blocks.push_back(&BB);
        }

        // Iterate over the collected basic blocks in reverse order
        for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
            BasicBlock *BB = *it;

            // Attempt to merge this block with its predecessor
            
            if (MergeBlockIntoPredecessor(BB)/*SafelyMergeBasicBlocks(BB,getFirstPredecessor(BB))*/) {
                MergeOccured = true;
            }
        }
    }while(MergeOccured);

    return true;
  }

}; // end of struct Constant Folding
}  // end of anonymous namespace

char ConstantPropogation::ID = 0;
static RegisterPass<ConstantPropogation> X("constfold", "Constant Folding Pass",
                             true,
                             true /* Analysis Pass */);

char ConditionalConstantPropogation::ID = 0;
static RegisterPass<ConditionalConstantPropogation> X1("cond-constfold", "Constant Folding Pass",
                             true,
                             true /* Analysis Pass */);