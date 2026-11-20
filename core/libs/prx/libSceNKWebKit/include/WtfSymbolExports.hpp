#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WtfAbi {

struct SymbolImpl {
    StringImpl string;
    StringImpl* owner;
    unsigned hashForSymbolShiftedWithFlagCount;
    unsigned flags;
};

struct RegisteredSymbolImpl {
    SymbolImpl symbol;
    void* symbolRegistry;
};

struct SymbolRegistry {
    void* table;
    std::uint8_t symbolType;
};

static_assert(std::is_standard_layout_v<SymbolImpl>);
static_assert(offsetof(SymbolImpl, owner) == 24);
static_assert(offsetof(SymbolImpl, hashForSymbolShiftedWithFlagCount) == 32);
static_assert(offsetof(SymbolImpl, flags) == 36);
static_assert(sizeof(SymbolImpl) == 40);
static_assert(offsetof(RegisteredSymbolImpl, symbolRegistry) == 40);
static_assert(sizeof(RegisteredSymbolImpl) == 48);
static_assert(offsetof(SymbolRegistry, symbolType) == 8);
static_assert(sizeof(SymbolRegistry) == 16);

constexpr unsigned SymbolFlagIsNullSymbol = 1;
constexpr unsigned SymbolFlagIsRegistered = 2;
constexpr unsigned SymbolFlagIsPrivate = 4;

}  // namespace NKWebKit::WtfAbi

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10SymbolImpl6createERNS_10StringImplE(NKWebKit::WtfAbi::StringImpl* rep);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF10SymbolImpl16createNullSymbolEv();
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF17PrivateSymbolImpl6createERNS_10StringImplE(NKWebKit::WtfAbi::StringImpl* rep);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE(NKWebKit::WtfAbi::SymbolRegistry* self, const NKWebKit::WtfAbi::String* key);
void APS5_VABI _ZN3WTF14SymbolRegistryD1Ev(NKWebKit::WtfAbi::SymbolRegistry* self);
void APS5_VABI _ZN3WTF14SymbolRegistryD2Ev(NKWebKit::WtfAbi::SymbolRegistry* self);

}
