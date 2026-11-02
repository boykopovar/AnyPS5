#include "ModuleLifter.hpp"

#include "FunctionLifter.hpp"

#include <lifter/Hex.hpp>

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <stdexcept>
#include <string>

namespace Ps5Lift {

namespace Lifting {

namespace {

std::string GuestSymbol(const std::uint64_t address) {
    return "guest_" + Hex(address);
}

}

llvm::Function* DeclareGuest(llvm::Module& module, const std::uint64_t address) {
    const auto symbol = GuestSymbol(address);
    if (auto* function = module.getFunction(symbol)) return function;
    auto* pointer = llvm::PointerType::get(module.getContext(), 0);
    auto* function = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(module.getContext()), {pointer}, false), llvm::GlobalValue::ExternalLinkage, symbol, module);
    function->setVisibility(llvm::GlobalValue::HiddenVisibility);
    function->addParamAttr(0, llvm::Attribute::NoAlias);
    function->addParamAttr(0, llvm::Attribute::NoUndef);
    return function;
}

std::string ImportName(const LiftOptions& options, const std::string& name) {
    const auto runtimeName = JumpBufferFunction(name).value_or(name);
    return options.ImportNids ? Nid(runtimeName) : runtimeName;
}

std::unique_ptr<llvm::Module> Verified(std::unique_ptr<llvm::Module> module) {
    std::string errors;
    llvm::raw_string_ostream stream(errors);
    if (llvm::verifyModule(*module, &stream)) throw std::runtime_error("Lifted module does not verify: " + stream.str());
    return module;
}

void ModuleLifter::LiftFunction(const GuestFunction& function) {
    FunctionLifter(*this, Decode(_image, function)).Lift();
}

}

std::unique_ptr<llvm::Module> LiftFunctions(llvm::LLVMContext& context, const GuestImage& image, const std::map<std::uint64_t, GuestFunction>& functions, const std::span<const std::uint64_t> addresses,
                                            const LiftOptions& options, const Target& target, LiftReport& report) {
    return Lifting::ModuleLifter(context, image, functions, options, target, report).LiftFunctions(addresses);
}

}
