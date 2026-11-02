#include <lifter/Target.hpp>

#include "LlvmCompatibility.hpp"

#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <mutex>
#include <set>
#include <stdexcept>
#include <string_view>

namespace Ps5Lift {

namespace {

void InitializeTargets() {
    static std::once_flag once;
    std::call_once(once, [] {
        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64Target();
        LLVMInitializeAArch64TargetMC();
        LLVMInitializeAArch64AsmPrinter();
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86AsmPrinter();
        auto& options = llvm::cl::getRegisteredOptions();
        const auto deinterleaving = options.find("enable-complex-deinterleaving");
        if (deinterleaving == options.end()) throw std::logic_error("LLVM has no enable-complex-deinterleaving option to turn the pass off");
        static_cast<llvm::cl::opt<bool>*>(deinterleaving->second)->setValue(false);
    });
}

std::unique_ptr<llvm::TargetMachine> CreateTargetMachine(const llvm::Triple& triple, const std::string& cpu, const std::string& features, const bool optimize) {
    InitializeTargets();
    std::string error;
    const auto* target = llvm::TargetRegistry::lookupTarget(TripleArgument(triple), error);
    if (target == nullptr) throw std::runtime_error("Unknown target " + triple.str() + ": " + error);
    llvm::TargetOptions targetOptions;
    auto* machine = target->createTargetMachine(TripleArgument(triple), cpu, features, targetOptions, llvm::Reloc::PIC_, std::nullopt, optimize ? llvm::CodeGenOptLevel::Aggressive : llvm::CodeGenOptLevel::None);
    if (machine == nullptr) throw std::runtime_error("Cannot create a target machine for " + triple.str());
    return std::unique_ptr<llvm::TargetMachine>(machine);
}

void Validate(const llvm::Triple& triple, const std::string& cpu, const std::string& features) {
    const auto generic = CreateTargetMachine(triple, "", "", false);
    const auto& subtarget = SubtargetInfo(*generic);
    if (!subtarget.isCPUStringValid(cpu)) throw std::runtime_error(cpu + " is not a CPU of " + triple.str());
    std::set<std::string_view> known;
    for (const auto& feature : subtarget.getAllProcessorFeatures()) known.insert(FeatureName(feature));
    if (features.empty()) return;
    std::string_view rest = features;
    while (true) {
        const auto comma = rest.find(',');
        const auto feature = rest.substr(0, comma);
        if (feature.size() < 2 || (feature[0] != '+' && feature[0] != '-')) throw std::runtime_error("features are +name or -name separated by commas, not '" + std::string(feature) + "'");
        if (!known.contains(feature.substr(1))) throw std::runtime_error(std::string(feature.substr(1)) + " is not a feature of " + triple.str());
        if (comma == std::string_view::npos) return;
        rest.remove_prefix(comma + 1);
    }
}

std::string HostFeatures() {
    std::string host;
    for (const auto& feature : HostCpuFeatures()) host += (host.empty() ? "" : ",") + std::string(feature.getValue() ? "+" : "-") + feature.getKey().str();
    return host;
}

}

std::unique_ptr<llvm::TargetMachine> Target::Machine() const {
    return CreateTargetMachine(Triple, Cpu, Features, Optimize);
}

void Target::Configure(llvm::Module& module) const {
    module.setTargetTriple(TripleArgument(Triple));
    module.setDataLayout(DataLayout);
}

Target ResolveTarget(const llvm::Triple& triple, const std::string& cpu, const std::string& features, const bool optimize) {
    if (!triple.isAArch64() && triple.getArch() != llvm::Triple::x86_64) throw std::runtime_error("ps5lift lifts for aarch64 and x86-64, not " + triple.str());
    Target target{triple, cpu, features, "", optimize};
    if (cpu == "native") {
        if (llvm::Triple(llvm::sys::getProcessTriple()).getArch() != triple.getArch())
            throw std::runtime_error("--cpu native describes this " + llvm::sys::getProcessTriple() + " host, which cannot run " + triple.str() + " code");
        target.Cpu = llvm::sys::getHostCPUName().str();
        const auto host = HostFeatures();
        target.Features = features.empty() ? host : host + "," + features;
    } else if (cpu.empty()) {
        target.Cpu = triple.isAArch64() ? "neoverse-v2" : "generic";
    }
    Validate(triple, target.Cpu, target.Features);
    const std::string required = triple.isAArch64() ? "+aes,+crc" : "+aes,+pclmul,+sse4.2,+crc32";
    target.Features = target.Features.empty() ? required : target.Features + "," + required;
    target.DataLayout = target.Machine()->createDataLayout().getStringRepresentation();
    return target;
}

}
