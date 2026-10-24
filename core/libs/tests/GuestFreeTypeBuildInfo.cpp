#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_MODULE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using FreeType::GuestLibrary;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(GuestLibrary**);
int APS5_VABI FT_Done_FreeType_nid_postfix(GuestLibrary*);
void APS5_VABI FT_Library_Version_nid_postfix(GuestLibrary*, std::int32_t*, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI FT_Get_TrueType_Engine_Type_nid_postfix(GuestLibrary*);
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "FreeType build info check failed at line %u\n", location.line());
        std::abort();
    }
}

void CheckVersion(GuestLibrary* guest, FT_Library native) {
    std::array<FT_Int, 3> expected{};
    FT_Library_Version(native, &expected[0], &expected[1], &expected[2]);
    for (unsigned mask = 0; mask < 8; ++mask) {
        std::array<std::int32_t, 5> values{0x12345678, -1, -2, -3, 0x76543210};
        const auto before = values;
        FT_Library_Version_nid_postfix(guest, mask & 1 ? &values[1] : nullptr,
            mask & 2 ? &values[2] : nullptr, mask & 4 ? &values[3] : nullptr);
        Require(values[0] == before[0] && values[4] == before[4]);
        for (unsigned index = 0; index < 3; ++index)
            Require(values[index + 1] == (mask & (1u << index) ? expected[index] : before[index + 1]));
        std::array<std::int32_t, 3> alias{0x12345678, -1, 0x76543210};
        FT_Library_Version_nid_postfix(guest, mask & 1 ? &alias[1] : nullptr,
            mask & 2 ? &alias[1] : nullptr, mask & 4 ? &alias[1] : nullptr);
        const auto last = mask & 4 ? expected[2] : mask & 2 ? expected[1] : mask & 1 ? expected[0] : -1;
        Require(alias[1] == last && alias[0] == 0x12345678 && alias[2] == 0x76543210);
    }
}

int main() {
    CheckVersion(nullptr, nullptr);
    Require(FT_Get_TrueType_Engine_Type_nid_postfix(nullptr) == FT_TRUETYPE_ENGINE_TYPE_NONE);
    GuestLibrary* first = nullptr;
    GuestLibrary* second = nullptr;
    FT_Library native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&first) == 0);
    Require(FT_Init_FreeType_nid_postfix(&second) == 0);
    Require(FT_Init_FreeType(&native) == 0);
    CheckVersion(first, native);
    CheckVersion(second, native);
    const auto initialEngine = FT_Get_TrueType_Engine_Type(native);
    Require(FT_Get_TrueType_Engine_Type_nid_postfix(first) == initialEngine);
    Require(FT_Get_TrueType_Engine_Type_nid_postfix(second) == initialEngine);
    if (const auto module = FT_Get_Module(native, "truetype")) {
        Require(FT_Remove_Module(first->host, FT_Get_Module(first->host, "truetype")) == 0);
        Require(FT_Remove_Module(native, module) == 0);
        Require(FT_Get_TrueType_Engine_Type_nid_postfix(first) == FT_TRUETYPE_ENGINE_TYPE_NONE);
        Require(FT_Get_TrueType_Engine_Type_nid_postfix(first) == FT_Get_TrueType_Engine_Type(native));
        Require(FT_Get_TrueType_Engine_Type_nid_postfix(second) == initialEngine);
        CheckVersion(first, native);
    }
    Require(FT_Done_FreeType_nid_postfix(first) == 0);
    CheckVersion(second, native);
    Require(FT_Get_TrueType_Engine_Type_nid_postfix(second) == initialEngine);
    Require(FT_Done_FreeType_nid_postfix(second) == 0);
    Require(FT_Done_FreeType(native) == 0);
}
