#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include <freetype/internal/ftobjs.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType OpenType debug hook: %s\n", message);
    std::exit(1);
}

void APS5_VABI FirstHook(void*) {}

void APS5_VABI SecondHook(void*) {}

FT_DebugHook_Func Host(void (APS5_VABI *hook)(void*)) {
    return reinterpret_cast<FT_DebugHook_Func>(hook);
}

FT_DebugHook_Func Stored(const FreeType::GuestLibrary* library, std::uint32_t index) {
    return library->host->debug_hooks[index];
}

bool NoneStored(const FreeType::GuestLibrary* library) {
    for (std::uint32_t index = 0; index < 4; ++index)
        if (Stored(library, index)) return false;
    return true;
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    FreeType::GuestLibrary* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && library, "FT_Init_FreeType creates a library");
    Require(NoneStored(library), "a new library has no hooks");

    FT_Set_Debug_Hook_nid_postfix(nullptr, 2, &FirstHook);
    FT_Set_Debug_Hook_nid_postfix(library, 2, nullptr);
    FT_Set_Debug_Hook_nid_postfix(library, 0, nullptr);
    FT_Set_Debug_Hook_nid_postfix(library, 4, &FirstHook);
    FT_Set_Debug_Hook_nid_postfix(library, 0xffffffffu, &FirstHook);
    Require(NoneStored(library), "a null hook or an index past the table changes nothing");

    FT_Set_Debug_Hook_nid_postfix(library, 1, &FirstHook);
    FT_Set_Debug_Hook_nid_postfix(library, 2, &FirstHook);
    FT_Set_Debug_Hook_nid_postfix(library, 3, &SecondHook);
    Require(Stored(library, 1) == Host(&FirstHook), "hook 1 is stored");
    Require(Stored(library, 2) == Host(&FirstHook), "hook 2 is stored");
    Require(Stored(library, 3) == Host(&SecondHook), "hook 3 is stored");

    FT_Set_Debug_Hook_nid_postfix(library, 2, nullptr);
    Require(Stored(library, 2) == Host(&FirstHook), "a null hook keeps the stored one");
    FT_Set_Debug_Hook_nid_postfix(library, 2, &SecondHook);
    Require(Stored(library, 2) == Host(&SecondHook), "a later hook replaces the stored one");

    Require(Throws([&] { FT_Set_Debug_Hook_nid_postfix(library, 0, &FirstHook); }), "the TrueType interpreter hook throws");
    Require(!Stored(library, 0), "a refused TrueType interpreter hook is not stored");

    Require(FT_Done_FreeType_nid_postfix(library) == 0, "FT_Done_FreeType releases the library");
    std::puts("Guest FreeType OpenType debug hook tests passed");
    return 0;
}
