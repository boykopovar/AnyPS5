#include "prx/libSceNKWebKit/include/WtfThreadFlagsExports.hpp"

namespace {

constexpr unsigned GCThreadTypeMask = 3;

thread_local bool currentIsCompilationThread = false;
thread_local unsigned currentGCThreadType = static_cast<unsigned>(NKWebKit::WtfAbi::GCThreadType::None);

}  // namespace

extern "C" {

bool APS5_VABI _ZN3WTF6Thread27exchangeIsCompilationThreadEb(bool newValue) {
    const bool oldValue = currentIsCompilationThread;
    currentIsCompilationThread = newValue;
    return oldValue;
}

bool APS5_VABI _ZN3WTF19isCompilationThreadEv() {
    return currentIsCompilationThread;
}

void APS5_VABI _ZN3WTF6Thread16registerGCThreadENS_12GCThreadTypeE(NKWebKit::WtfAbi::GCThreadType gcThreadType) {
    currentGCThreadType = static_cast<unsigned>(gcThreadType) & GCThreadTypeMask;
}

bool APS5_VABI _ZN3WTF6Thread13mayBeGCThreadEv() {
    return static_cast<NKWebKit::WtfAbi::GCThreadType>(currentGCThreadType) != NKWebKit::WtfAbi::GCThreadType::None;
}

}
