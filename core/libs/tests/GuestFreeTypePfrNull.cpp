#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_FONT_FORMATS_H
#include FT_PFR_H
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

unsigned imports = 0;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "PFR null face: %s\n", message);
    std::exit(1);
}

void CheckError(int result, const char* operation) {
    if (result == 6) return;
    std::fprintf(stderr, "PFR null face: %s returned %d, expected 6; host imports %u\n", operation, result, imports);
    std::exit(1);
}

template<class T>
struct Guarded {
    std::uint64_t before = UINT64_C(0x123456789abcdef0);
    T value;
    std::uint64_t after = UINT64_C(0xfedcba9876543210);

    void Verify() const {
        Require(before == UINT64_C(0x123456789abcdef0) && after == UINT64_C(0xfedcba9876543210), "output canaries");
    }
};

}

namespace FreeType {

GuestFace* FaceOf(const GuestFaceRec* face) {
    Require(face == nullptr, "fixture received a nonnull face");
    return nullptr;
}

}

extern "C" {

FT_Error FT_Get_PFR_Advance(FT_Face, FT_UInt, FT_Pos*) {
    ++imports;
    return FT_Err_Invalid_Face_Handle;
}

FT_Error FT_Get_PFR_Kerning(FT_Face, FT_UInt, FT_UInt, FT_Vector*) {
    ++imports;
    return FT_Err_Invalid_Face_Handle;
}

FT_Error FT_Get_PFR_Metrics(FT_Face, FT_UInt*, FT_UInt*, FT_Fixed*, FT_Fixed*) {
    ++imports;
    return FT_Err_Invalid_Face_Handle;
}

const char* FT_Get_Font_Format(FT_Face) {
    ++imports;
    return nullptr;
}

}

int main() {
    Guarded<std::uint32_t> outline{.value = 0xdeadbeefu}, metrics{.value = 0xa5a5a5a5u};
    Guarded<std::int64_t> x{.value = INT64_C(0x123456789abc)}, y{.value = -INT64_C(0x123456789abc)};
    for (unsigned mask = 0; mask < 16; ++mask) {
        CheckError(FT_Get_PFR_Metrics_nid_postfix(nullptr, mask & 1 ? &outline.value : nullptr,
            mask & 2 ? &metrics.value : nullptr, mask & 4 ? &x.value : nullptr, mask & 8 ? &y.value : nullptr), "metrics");
        Require(outline.value == 0xdeadbeefu && metrics.value == 0xa5a5a5a5u &&
            x.value == INT64_C(0x123456789abc) && y.value == -INT64_C(0x123456789abc), "metrics output unchanged");
        outline.Verify(); metrics.Verify(); x.Verify(); y.Verify();
    }
    Guarded<std::int64_t> advance{.value = INT64_C(0x123456789abc)};
    Guarded<FreeType::GuestVector> vector{.value = {INT64_C(0x123456789abc), -INT64_C(0x123456789abc)}};
    for (const auto index : std::array<std::uint32_t, 3>{0, 1, UINT32_MAX}) {
        for (const bool output : {false, true}) {
            CheckError(FT_Get_PFR_Advance_nid_postfix(nullptr, index, output ? &advance.value : nullptr), "advance");
            CheckError(FT_Get_PFR_Kerning_nid_postfix(nullptr, index, UINT32_MAX - index, output ? &vector.value : nullptr), "kerning");
            Require(advance.value == INT64_C(0x123456789abc), "advance output unchanged");
            Require(vector.value.x == INT64_C(0x123456789abc) && vector.value.y == -INT64_C(0x123456789abc), "kerning output unchanged");
            advance.Verify(); vector.Verify();
        }
    }
    Require(imports == 0, "null face called a host import");
    std::puts("PFR null-face contract passed without host imports");
}
