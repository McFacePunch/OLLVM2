#include <sstream>
#include <string>
#include <random>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/Transforms/Obfuscation/StringObfuscation.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "llvm/Support/SourceMgr.h"


using namespace llvm;

static cl::opt<bool> StringObf("sobf", cl::init(false),
                               cl::desc("Enable the string obfuscation"));

#define DEBUG_TYPE "objdiv"

STATISTIC(GlobalsEncoded, "Counts number of global variables encoded");

namespace obf_inner {
void flatten(Function *f);
void substitution(Function *f);
void split(Function *f);
void bogus(Function *f);
} // namespace obf_inner

namespace llvm {

struct encVar {
public:
  GlobalVariable *var;
  uint8_t key_xor;
  uint8_t key_cesar;
};

class StringObfuscationPass : public llvm::ModulePass {
public:
  static char ID; // pass identification
  bool is_flag = false;
  StringObfuscationPass() : ModulePass(ID) {}
  StringObfuscationPass(bool flag) : ModulePass(ID) { is_flag = flag;}

  virtual bool runOnModule(Module &M) override {
    if (!is_flag)
        return false;

    std::vector<encVar *> encGlob;

    for (auto &GV : M.globals()) {
        GlobalVariable *gv = &GV;

        if (!isStringConstant(gv)) {
            errs() << "Skipping global variable: " << gv->getName() << "\n";
            continue;
        }

        outs() << "========================================================" << "\n";
        outs() << "Working on global variable: " << gv->getName() << "\n";

        ConstantDataSequential *cdata = dyn_cast<ConstantDataSequential>(gv->getInitializer());
        if (!cdata) {
            errs() << "Skipping inner obfuscation for global variable: " << gv->getName() << "\n";
            continue;
        }

        std::vector<uint8_t> encr(cdata->getNumElements());

        encVar *cur = new encVar();
        cur->var = gv;
        cur->key_xor = getRandomKey();
        cur->key_cesar = getRandomKey();

        for (unsigned i = 0; i < encr.size(); ++i) {
            encr[i] = cdata->getElementAsInteger(i) ^ cur->key_xor;
            encr[i] += cur->key_cesar;
        }

        Constant *initnew = ConstantDataArray::get(M.getContext(), encr);
        gv->setInitializer(initnew);

        encGlob.push_back(cur);
    }

    for (auto *enc : encGlob) {
        // Add an if here to choose between inline xor per byte OR looped xor decryption, long strings are too much for per byte since i^2 per decryption
        addDecodeInstructions(M, enc->var, enc);
    }

    return true;
    }


private:
    void addDecodeInstructions(Module &M, GlobalVariable *gvar, encVar *enc) {
        if (!gvar->getValueType()->isArrayTy()) {
            errs() << "Error: Global variable is not an array type. Cannot apply string obfuscation.\n";
            return;
        }

        LLVMContext &context = M.getContext();
        Type *int8Ty = Type::getInt8Ty(context);
        Type *int8PtrTy = PointerType::get(int8Ty, 0);

        ArrayType *arrayType = cast<ArrayType>(gvar->getValueType());
        unsigned len = arrayType->getNumElements();

        SmallVector<User *, 8> users(gvar->users().begin(), gvar->users().end());
        for (User *user : users) {
            if (Instruction *instr = dyn_cast<Instruction>(user)) {
                IRBuilder<> builder(instr);

                // Allocate memory on the stack for the decoded array
                AllocaInst *stackAlloc = builder.CreateAlloca(arrayType, nullptr, "stackAlloc");
                stackAlloc->setAlignment(Align(16));

                /////////////////
                // Loop over the global variable's array and decode the contents
                for (unsigned i = 0; i < len; ++i) {
                    Value *globalPtr = builder.CreateGEP(arrayType, gvar, {builder.getInt32(0), builder.getInt32(i)});
                    Value *stackPtr = builder.CreateGEP(arrayType, stackAlloc, {builder.getInt32(0), builder.getInt32(i)});

                    // Load from global variable, decrypt, and store in stack
                    Value *loadedValue = builder.CreateLoad(int8Ty, globalPtr);
                    Value *decodedValue = builder.CreateXor(builder.CreateSub(loadedValue, builder.getInt8(enc->key_cesar)), builder.getInt8(enc->key_xor));
                    builder.CreateStore(decodedValue, stackPtr);
                }

                // Replace all uses of the global variable with the pointer to the decoded stack memory
                instr->replaceUsesOfWith(gvar, builder.CreatePointerCast(stackAlloc, int8PtrTy));

                IRBuilder<> postbuilder(instr->getNextNonDebugInstruction());

                ////////////////////
                // Second loop: Overwrite the stack data to zero it out
                for (unsigned i = 0; i < len; ++i) {
                    Value *stackPtr = postbuilder.CreateGEP(arrayType, stackAlloc, {postbuilder.getInt32(0), postbuilder.getInt32(i)});
                    postbuilder.CreateStore(builder.getInt8(0), stackPtr);
                }
            }
        }

        // If there are no remaining users of the global variable, erase it
        if (gvar->use_empty()) {
            gvar->eraseFromParent();
        } else {
            errs() << "Failed to erase global variable: " << gvar->getName() << "\n";
        }
    }


    bool isStringConstant(GlobalVariable *gv) {
        std::string section(gv->getSection());

        if (section == "llvm.metadata" || section.find("__objc_methname") != std::string::npos) {
            return false;
        }

        if (!gv->isConstant() || !gv->hasInitializer()) {
            return false;
        }

        if (auto *cdata = dyn_cast<ConstantDataSequential>(gv->getInitializer())) {
            if (cdata->isString() && cdata->getElementByteSize() == 1 && cdata->getElementType()->isIntegerTy(8)) {
                return true;
            }
        }

        if (isa<ConstantStruct>(gv->getInitializer()) || isa<ConstantAggregate>(gv->getInitializer())) {
            for (unsigned i = 0, e = gv->getInitializer()->getNumOperands(); i != e; ++i) {
                Value *operand = gv->getInitializer()->getOperand(i);

                if (auto *cdata = dyn_cast<ConstantDataSequential>(operand)) {
                    if (cdata->isString() && cdata->getElementByteSize() == 1 && cdata->getElementType()->isIntegerTy(8)) {
                        return true;
                    }
                }

                if (auto *nestedGV = dyn_cast<GlobalVariable>(operand)) {
                    if (isStringConstant(nestedGV)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    uint8_t getRandomKey() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 127); // Generates random keys in the range [1, 127]
        return static_cast<uint8_t>(dis(gen));
    }


};
}; // namespace


llvm::PreservedAnalyses llvm::StringObfuscationNewPass::run(llvm::Module &M,
                                    llvm::ModuleAnalysisManager &AM) {
  llvm::StringObfuscationPass sobf(StringObf);
  if (sobf.runOnModule(M))
    return PreservedAnalyses::all();

  return PreservedAnalyses::none();
}

char StringObfuscationPass::ID = 0;
static RegisterPass<StringObfuscationPass>
    X("GVDiv", "Global variable (i.e., const char*) diversification pass", false, true);

Pass *llvm::createStringObfuscation(bool flag) {
  return new StringObfuscationPass(flag);
}


