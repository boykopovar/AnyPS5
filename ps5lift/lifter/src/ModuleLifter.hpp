#ifndef PS5LIFT_MODULELIFTER_HPP
#define PS5LIFT_MODULELIFTER_HPP

#include <lifter/Discovery.hpp>
#include <lifter/GuestImage.hpp>
#include <lifter/Hex.hpp>
#include <lifter/Lifter.hpp>
#include <lifter/Target.hpp>

#include <ps5lift/State.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace Ps5Lift::Lifting {

llvm::Function* DeclareGuest(llvm::Module& module, std::uint64_t address);
std::string ImportName(const LiftOptions& options, const std::string& name);
std::unique_ptr<llvm::Module> Verified(std::unique_ptr<llvm::Module> module);

class ModuleLifter {
public:
    ModuleLifter(llvm::LLVMContext& context, const GuestImage& image, const std::map<std::uint64_t, GuestFunction>& functions, const LiftOptions& options, const Ps5Lift::Target& target,
                 LiftReport& report)
        : _context(context), _image(image), _functions(functions), _options(options), _target(target), _report(report), _module(std::make_unique<llvm::Module>("ps5lift", context)) {
        _target.Configure(*_module);
        llvm::MDBuilder metadata(_context);
        auto* root = metadata.createTBAARoot("ps5lift");
        auto* stateType = metadata.createTBAAScalarTypeNode("state", root);
        auto* guestType = metadata.createTBAAScalarTypeNode("guest", root);
        _stateTag = metadata.createTBAAStructTagNode(stateType, stateType, 0);
        _guestTag = metadata.createTBAAStructTagNode(guestType, guestType, 0);
        _pointer = llvm::PointerType::get(_context, 0);
        _guestType = llvm::FunctionType::get(llvm::Type::getVoidTy(_context), {_pointer}, false);
    }

    std::unique_ptr<llvm::Module> LiftFunctions(const std::span<const std::uint64_t> addresses) {
        for (const auto address : addresses) {
            const auto& function = _functions.at(address);
            if (function.Import) LiftImport(function);
            else LiftFunction(function);
            ++_report.Functions;
        }
        return Verified(std::move(_module));
    }

    llvm::Function* Guest(const std::uint64_t address) {
        if (!_functions.contains(address)) throw std::runtime_error("Call to undiscovered guest function 0x" + Hex(address));
        return DeclareGuest(*_module, address);
    }

    llvm::FunctionCallee Runtime(const std::string& name, llvm::FunctionType* type) {
        return _module->getOrInsertFunction(name, type);
    }

    llvm::LLVMContext& Context() { return _context; }
    llvm::MDNode* StateTag() const { return _stateTag; }
    llvm::MDNode* GuestTag() const { return _guestTag; }
    llvm::PointerType* Pointer() const { return _pointer; }
    llvm::FunctionType* GuestType() const { return _guestType; }
    const LiftOptions& Options() const { return _options; }
    const llvm::Triple& Triple() const { return _target.Triple; }

    std::string ImportName(const std::string& name) const { return Lifting::ImportName(_options, name); }
    LiftReport& Report() { return _report; }
    const std::map<std::uint64_t, GuestFunction>& Functions() const { return _functions; }
    const GuestImage& Image() const { return _image; }

private:
    llvm::LLVMContext& _context;
    const GuestImage& _image;
    const std::map<std::uint64_t, GuestFunction>& _functions;
    const LiftOptions& _options;
    const Ps5Lift::Target& _target;
    LiftReport& _report;
    std::unique_ptr<llvm::Module> _module;
    llvm::PointerType* _pointer = nullptr;
    llvm::FunctionType* _guestType = nullptr;
    llvm::MDNode* _stateTag = nullptr;
    llvm::MDNode* _guestTag = nullptr;

    void LiftFunction(const GuestFunction& function);

    void LiftImport(const GuestFunction& function) {
        auto* host = Guest(function.Entry);
        llvm::IRBuilder<> builder(llvm::BasicBlock::Create(_context, "entry", host));
        auto* state = host->getArg(0);
        builder.CreateCall(Runtime(BridgeSymbol(ImportName(*function.Import)), _guestType), {state});
        auto* unwinding = builder.CreateLoad(builder.getInt8Ty(), builder.CreateConstGEP1_64(builder.getInt8Ty(), state, offsetof(Ps5LiftState, Unwinding)));
        unwinding->setMetadata(llvm::LLVMContext::MD_tbaa, _stateTag);
        auto* returned = llvm::BasicBlock::Create(_context, "returned", host);
        auto* unwind = llvm::BasicBlock::Create(_context, "unwind", host);
        builder.CreateCondBr(builder.CreateICmpNE(unwinding, builder.getInt8(0)), unwind, returned, llvm::MDBuilder(_context).createBranchWeights(1, 1u << 20));
        llvm::IRBuilder<>(unwind).CreateRetVoid();
        builder.SetInsertPoint(returned);
        auto* rsp = builder.CreateConstGEP1_64(builder.getInt8Ty(), state, offsetof(Ps5LiftState, Gpr) + 8 * PS5LIFT_RSP);
        auto* load = builder.CreateLoad(builder.getInt64Ty(), rsp);
        load->setMetadata(llvm::LLVMContext::MD_tbaa, _stateTag);
        builder.CreateStore(load, builder.CreateConstGEP1_64(builder.getInt8Ty(), state, offsetof(Ps5LiftState, ReturnSlot)))->setMetadata(llvm::LLVMContext::MD_tbaa, _stateTag);
        builder.CreateStore(builder.CreateAdd(load, builder.getInt64(8)), rsp)->setMetadata(llvm::LLVMContext::MD_tbaa, _stateTag);
        builder.CreateRetVoid();
    }

};

}

#endif
