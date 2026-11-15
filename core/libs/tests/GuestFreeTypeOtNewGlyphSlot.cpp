#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using FreeType::GuestFace;
using FreeType::GuestFaceRec;
using FreeType::GuestGlyphSlotRec;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot New_GlyphSlot: %s\n", message);
    std::exit(1);
}

void NullFace() {
    GuestGlyphSlotRec* aslot = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(nullptr, &aslot) == 0x06, "a null face returns Invalid_Argument");
    Require(aslot == nullptr, "a null face leaves the slot pointer unchanged");
}

void NullDriver() {
    FT_FaceRec host{};
    GuestFace face;
    face.host = &host;
    GuestFaceRec rec{};
    rec.internal = &face;
    GuestGlyphSlotRec* aslot = nullptr;
    Require(FT_New_GlyphSlot_nid_postfix(&rec, &aslot) == 0x06, "a face without a driver returns Invalid_Argument");
    Require(aslot == nullptr, "a face without a driver leaves the slot pointer unchanged");
}

}

int main() {
    NullFace();
    NullDriver();
    std::puts("Guest FreeType Ot New_GlyphSlot tests passed");
    return 0;
}
