#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleIsLoadedInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret);
int APS5_VABI sceSysmoduleUnloadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret);
}

namespace {

constexpr int kInvalidValue = static_cast<int>(0x805A1000);

void Require(bool value) { if (!value) std::abort(); }

void CheckPublicIdRejected(std::uint16_t id) {
    Require(sceSysmoduleIsLoaded(id) == kInvalidValue);
    Require(sceSysmoduleLoadModule(id) == kInvalidValue);
    Require(sceSysmoduleIsLoaded(id) == kInvalidValue);
    Require(sceSysmoduleUnloadModule(id) == kInvalidValue);
    Require(sceSysmoduleUnloadModule(id) == kInvalidValue);
}

void CheckInternalIdRejected(std::uint32_t id) {
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
    Require(sceSysmoduleLoadModuleInternal(id) == kInvalidValue);
    Require(sceSysmoduleUnloadModuleInternal(id) == kInvalidValue);
}

void CheckInternalIdRejectedWithArg(std::uint32_t id) {
    Require(sceSysmoduleLoadModuleInternalWithArg(id, 0, nullptr, 0, nullptr) == kInvalidValue);
    Require(sceSysmoduleUnloadModuleInternalWithArg(id, 0, nullptr, 0, nullptr) == kInvalidValue);
}

void CheckInternalIsLoadedRejected(std::uint32_t id) {
    Require(sceSysmoduleIsLoadedInternal(id) == kInvalidValue);
}

}

int main() {
    CheckPublicIdRejected(0x7fff);
    CheckPublicIdRejected(0xffff);
    CheckPublicIdRejected(0x4321);
    CheckInternalIdRejected(0x80ffff00);
    CheckInternalIdRejected(0x8000ffff);
    CheckInternalIsLoadedRejected(0);
    CheckInternalIsLoadedRejected(0x80000000);
    CheckInternalIsLoadedRejected(0x00ffffff);
    CheckInternalIdRejectedWithArg(0x80ffff00);
    CheckInternalIdRejectedWithArg(0x8000ffff);
}
