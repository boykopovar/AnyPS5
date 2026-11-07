#ifndef PS5LIFT_LLVM_COMPATIBILITY_HPP
#define PS5LIFT_LLVM_COMPATIBILITY_HPP

#include <llvm/ADT/StringMap.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace Ps5Lift {

#if LLVM_VERSION_MAJOR >= 21
inline const llvm::Triple& TripleArgument(const llvm::Triple& triple) {
    return triple;
}
#else
inline std::string TripleArgument(const llvm::Triple& triple) {
    return triple.str();
}
#endif

template <typename Machine>
const llvm::MCSubtargetInfo& SubtargetInfo(const Machine& machine) {
    if constexpr (std::is_pointer_v<decltype(machine.getMCSubtargetInfo())>) return *machine.getMCSubtargetInfo();
    else return machine.getMCSubtargetInfo();
}

template <typename Block>
bool HasTerminator(const Block& block) {
    if constexpr (requires { block.hasTerminator(); }) return block.hasTerminator();
    else return block.getTerminator() != nullptr;
}

template <typename Feature>
std::string_view FeatureName(const Feature& feature) {
    if constexpr (requires { feature.key(); }) return feature.key();
    else return feature.Key;
}

inline llvm::StringMap<bool> HostCpuFeatures() {
#if LLVM_VERSION_MAJOR >= 19
    return llvm::sys::getHostCPUFeatures();
#else
    llvm::StringMap<bool> features;
    llvm::sys::getHostCPUFeatures(features);
    return features;
#endif
}

}

#endif
