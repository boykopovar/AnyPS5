#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include "WtfObjectIdentifier.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace {

std::once_flag processIdentifierOnce;
std::atomic<bool> processIdentifierSet{false};
std::atomic<std::uint64_t> processIdentifier{0};

}

extern "C" std::uint64_t APS5_VABI _ZN7WebCore7Process10identifierEv() {
    std::call_once(processIdentifierOnce, [] {
        if (processIdentifierSet.load()) return;
        processIdentifierSet.store(true);
        processIdentifier.store(NKWebKit::WtfAbi::GenerateObjectIdentifier());
    });
    return processIdentifier.load();
}

extern "C" void APS5_VABI _ZN7WebCore7Process13setIdentifierEN3WTF23ObjectIdentifierGenericINS_21ProcessIdentifierTypeENS1_38ObjectIdentifierMainThreadAccessTraitsEEE(std::uint64_t identifier) {
    processIdentifierSet.store(true);
    processIdentifier.store(identifier);
}

extern "C" void APS5_VABI _ZN7WebCore7Process13setIdentifierEN3WTF16ObjectIdentifierINS_21ProcessIdentifierTypeEEE(std::uint64_t identifier) {
    _ZN7WebCore7Process13setIdentifierEN3WTF23ObjectIdentifierGenericINS_21ProcessIdentifierTypeENS1_38ObjectIdentifierMainThreadAccessTraitsEEE(identifier);
}
