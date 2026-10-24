#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTOBJECTS_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTOBJECTS_HPP

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <list>
#include <memory>
#include <vector>

namespace FreeType {

struct GuestFace;

struct GuestLibrary {
    FT_Library host = nullptr;
    std::list<std::unique_ptr<GuestFace>> faces;
};

void UnregisterBitmap_nid_no_patch(const GuestBitmap* bitmap);

struct GuestFace {
    GuestFaceRec rec{};
    GuestGlyphSlotRec slot{};
    GuestSizeRec size{};
    GuestLibrary* library = nullptr;
    FT_Face host = nullptr;
    std::vector<unsigned char> bytes;
    std::vector<GuestBitmapSize> fixedSizes;
    std::vector<GuestCharMapRec> charmaps;
    std::vector<GuestCharMapRec*> charmapPointers;
    std::vector<GuestVector> outlinePoints;

    ~GuestFace() { UnregisterBitmap_nid_no_patch(&slot.bitmap); }
};

void RegisterBitmap_nid_no_patch(GuestFace& face);

GuestFace* FaceOf(const GuestFaceRec* face);
GuestFace* FaceOf(const GuestGlyphSlotRec* slot);
void SyncFace(GuestFace& face);
void SyncSize(GuestFace& face);
void SyncSlot(GuestFace& face);

}

#endif
