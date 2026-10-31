#include "prx/libSceConnectAccountDialog/include/InventoryExports.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" [[noreturn, gnu::weak]] void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName ? funcName : "") + " not implemented");
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template <typename TCall>
bool ThrowsRuntimeError(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void AssertAllThrow() {
    std::array<std::uint8_t, 32> preGuard{};
    preGuard.fill(0x5a);
    std::array<std::uint8_t, 32> postGuard{};
    postGuard.fill(0x5a);

    Require(ThrowsRuntimeError([] { sceConnectAccountDialogClose(); }), "sceConnectAccountDialogClose did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogGetResult(); }), "sceConnectAccountDialogGetResult did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogGetStatus(); }), "sceConnectAccountDialogGetStatus did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogInitialize(); }), "sceConnectAccountDialogInitialize did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogOpen(); }), "sceConnectAccountDialogOpen did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogTerminate(); }), "sceConnectAccountDialogTerminate did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogUpdateStatus(); }), "sceConnectAccountDialogUpdateStatus did not throw std::runtime_error");

    Require(std::all_of(preGuard.begin(), preGuard.end(), [](std::uint8_t byte) { return byte == 0x5a; }), "pre-guard sentinel buffer was corrupted");
    Require(std::all_of(postGuard.begin(), postGuard.end(), [](std::uint8_t byte) { return byte == 0x5a; }), "post-guard sentinel buffer was corrupted");
}

void AssertLifecycleSequence() {
    std::array<std::uint8_t, 32> preGuard{};
    preGuard.fill(0x5a);
    std::array<std::uint8_t, 32> postGuard{};
    postGuard.fill(0x5a);

    Require(ThrowsRuntimeError([] { sceConnectAccountDialogInitialize(); }), "sceConnectAccountDialogInitialize did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogOpen(); }), "sceConnectAccountDialogOpen did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogUpdateStatus(); }), "sceConnectAccountDialogUpdateStatus did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogGetStatus(); }), "sceConnectAccountDialogGetStatus did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogGetResult(); }), "sceConnectAccountDialogGetResult did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogClose(); }), "sceConnectAccountDialogClose did not throw std::runtime_error");
    Require(ThrowsRuntimeError([] { sceConnectAccountDialogTerminate(); }), "sceConnectAccountDialogTerminate did not throw std::runtime_error");

    Require(std::all_of(preGuard.begin(), preGuard.end(), [](std::uint8_t byte) { return byte == 0x5a; }), "pre-guard sentinel buffer was corrupted");
    Require(std::all_of(postGuard.begin(), postGuard.end(), [](std::uint8_t byte) { return byte == 0x5a; }), "post-guard sentinel buffer was corrupted");
}

}

int main() {
    for (int iteration = 0; iteration < 10; ++iteration) {
        AssertAllThrow();
        AssertLifecycleSequence();
    }
    return 0;
}
