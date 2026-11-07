#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module = llvm::parseIRFile(argv[1], diagnostic, context);
    if (!module) {
        diagnostic.print(argv[0], llvm::errs());
        return 1;
    }
    if (module->empty() || llvm::verifyModule(*module, &llvm::errs())) {
        return 1;
    }
    return 0;
}
