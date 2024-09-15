#include <set> 

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <memory>
#include <random>
#include <map>  
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"        
#include "llvm/IR/Instruction.h"    
#include "llvm/IR/InstIterator.h"    
#include "llvm/Transforms/Utils/ModuleUtils.h" 
#include "llvm/IR/GlobalVariable.h"

using namespace llvm;

// Command-line options
static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input .ll or .bc file>"), cl::Required);
static cl::opt<std::string> OutputFilename("o", cl::desc("Output file"), cl::value_desc("filename"), cl::init("-"));

struct encVar {
public:
  GlobalVariable *var;
  uint8_t key_xor;
  uint8_t key_cesar;
};

// Function to generate a random encryption key (missing feature added)
uint8_t getRandomKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 127); // Generates random keys in the range [1, 127]
    return static_cast<uint8_t>(dis(gen));
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






Function *createGlobalDecodeFunction(LLVMContext &context, Module &module, ArrayType *arrayType, encVar *enc, unsigned len, GlobalVariable *gvar) {
    // Create a function that decrypts the global variable in place
    FunctionType *funcType = FunctionType::get(Type::getVoidTy(context), false);
    Function *decodeFunc = Function::Create(funcType, Function::InternalLinkage, "decode_" + gvar->getName(), &module);

    BasicBlock *entry = BasicBlock::Create(context, "entry", decodeFunc);
    IRBuilder<> builder(entry);

    // Decrypt the global variable in place
    Value *gvarPtr = builder.CreateBitCast(gvar, arrayType->getPointerTo());

    // Loop over the elements
    AllocaInst *iDecode = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "iDecode");
    builder.CreateStore(builder.getInt32(0), iDecode);

    BasicBlock *loopCond = BasicBlock::Create(context, "loopCond", decodeFunc);
    BasicBlock *loopBody = BasicBlock::Create(context, "loopBody", decodeFunc);
    BasicBlock *afterLoop = BasicBlock::Create(context, "afterLoop", decodeFunc);

    builder.CreateBr(loopCond);

    // Loop condition
    builder.SetInsertPoint(loopCond);
    LoadInst *iValue = builder.CreateLoad(builder.getInt32Ty(), iDecode);
    Value *cond = builder.CreateICmpSLT(iValue, builder.getInt32(len));
    builder.CreateCondBr(cond, loopBody, afterLoop);

    // Loop body
    builder.SetInsertPoint(loopBody);
    // Get pointer to the i-th element
    Value *idxList[2] = {builder.getInt32(0), iValue};
    Value *elementPtr = builder.CreateGEP(arrayType, gvarPtr, idxList);
    // Load the encrypted byte
    Value *encryptedByte = builder.CreateLoad(builder.getInt8Ty(), elementPtr);
    // Decrypt
    Value *decryptedByte = builder.CreateXor(builder.CreateSub(encryptedByte, builder.getInt8(enc->key_cesar)), builder.getInt8(enc->key_xor));
    // Store back the decrypted byte
    builder.CreateStore(decryptedByte, elementPtr);
    // Increment i
    Value *iNext = builder.CreateAdd(iValue, builder.getInt32(1));
    builder.CreateStore(iNext, iDecode);
    // Jump back to loop condition
    builder.CreateBr(loopCond);

    // After loop
    builder.SetInsertPoint(afterLoop);
    builder.CreateRetVoid();

    return decodeFunc;
}




void addDecodeInstructions(LLVMContext &context, GlobalVariable *gvar, encVar *enc, Module &module) {
    if (!gvar->getValueType()->isArrayTy()) {
        errs() << "Error: Global variable is not an array type. Cannot apply string obfuscation.\n";
        return;
    }

    ArrayType *arrayType = cast<ArrayType>(gvar->getValueType());
    unsigned len = arrayType->getNumElements();

    // Create the helper function to decrypt the global variable
    Function *decodeFunc = createGlobalDecodeFunction(context, module, arrayType, enc, len, gvar);

    // Register the decode function to be called at startup
    appendToGlobalCtors(module, decodeFunc, 65535);  // Lower priority runs earlier

    // Ensure the global variable is writable
    gvar->setConstant(false);
}











bool runOnModule(Module &M) {
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
        addDecodeInstructions(M.getContext(), enc->var, enc, M);
    }

    return true;
}

void modifyModule(Module &M) {
    Function *mainFunc = M.getFunction("main");

    runOnModule(M);

    errs() << "[DEBUG] Completed modification of main function.\n";

    mainFunc->print(errs(), nullptr);
}

int main(int argc, char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "LLVM IR String Decryption Tool\n");

    LLVMContext context;
    SMDiagnostic err;

    std::unique_ptr<Module> M = parseIRFile(InputFilename, err, context);
    if (!M) {
        errs() << "[ERROR] Error reading file: " << InputFilename << "\n";
        err.print(argv[0], errs());
        return 1;
    }

    errs() << "[DEBUG] Successfully loaded module: " << InputFilename << "\n";

    modifyModule(*M);

    if (verifyModule(*M, &errs())) {
        errs() << "[ERROR] Module verification failed.\n";
        return 1;
    }

    errs() << "[DEBUG] Module verification succeeded.\n";

    std::error_code EC;
    raw_fd_ostream dest((OutputFilename.getValue() == "-") ? "-" : OutputFilename.c_str(), EC, sys::fs::OF_None);
    if (EC) {
        errs() << "[ERROR] Error opening file: " << EC.message() << "\n";
        return 1;
    }
    M->print(dest, nullptr);

    errs() << "[DEBUG] Successfully wrote the modified module to " << OutputFilename << "\n";

    return 0;
}
