#include "prx/libSceVsePlugin/libSceVsePlugin.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int invalidHandleError = static_cast<int>(0x81b00001);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    auto* fakeHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1000));
    std::uint64_t dummyArg = 0x12345678u;

    Require(sceVsePluginUnknown33() == 0x100000);

    Require(sceVsePluginUnknown18(nullptr) == 0);
    Require(sceVsePluginUnknown18(fakeHandle) == 0);
    Require(sceVsePluginUnknown25(nullptr) == 0);
    Require(sceVsePluginUnknown25(fakeHandle) == 0);
    Require(sceVsePluginUnknown26(nullptr, nullptr) == 0);
    Require(sceVsePluginUnknown26(fakeHandle, &dummyArg) == 0);
    Require(sceVsePluginUnknown27(nullptr, nullptr) == 0);
    Require(sceVsePluginUnknown27(fakeHandle, &dummyArg) == 0);

    Require(sceVsePluginUnknown01(nullptr) == invalidHandleError);
    Require(sceVsePluginUnknown01(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown02(nullptr, nullptr) == invalidHandleError);
    Require(sceVsePluginUnknown02(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown03(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown04(nullptr, nullptr) == invalidHandleError);
    Require(sceVsePluginUnknown04(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown05(fakeHandle, 0u, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown06(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown07(fakeHandle, 0u, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown08(fakeHandle, 0u, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown09(fakeHandle, 1u) == invalidHandleError);
    Require(sceVsePluginUnknown10(fakeHandle, 1u) == invalidHandleError);
    Require(sceVsePluginUnknown11(fakeHandle, 0u, 0u) == invalidHandleError);
    Require(sceVsePluginUnknown12(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown13(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown14(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown15(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown16(fakeHandle) == invalidHandleError);
    Require(sceVsePluginUnknown17(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown19(fakeHandle, 0u) == invalidHandleError);
    Require(sceVsePluginUnknown20(fakeHandle, 0u) == invalidHandleError);
    Require(sceVsePluginUnknown21(fakeHandle, 0u) == invalidHandleError);
    Require(sceVsePluginUnknown22(fakeHandle, 0u, 0u, 0u) == invalidHandleError);
    Require(sceVsePluginUnknown23(fakeHandle, &dummyArg, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown24(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown28(fakeHandle, &dummyArg, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown29(fakeHandle, &dummyArg, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown30(fakeHandle, &dummyArg) == invalidHandleError);
    Require(sceVsePluginUnknown31(fakeHandle, 0u) == invalidHandleError);
    return 0;
}
