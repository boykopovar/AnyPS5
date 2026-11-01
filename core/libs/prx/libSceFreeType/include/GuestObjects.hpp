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

extern "C" void UnregisterBitmap_nid_no_patch(const GuestBitmap* bitmap);

struct GuestSize {
    GuestSizeRec rec{};
    FT_Size host = nullptr;
};

struct GuestFace {
    GuestFaceRec rec{};
    GuestGlyphSlotRec slot{};
    GuestSizeRec size{};
    GuestLibrary* library = nullptr;
    FT_Face host = nullptr;
    bool finalized = false;
    FT_Size initialSize = nullptr;
    bool sizeInitialized = false;
    std::list<GuestSize> sizes;
    std::vector<unsigned char> bytes;
    std::vector<GuestBitmapSize> fixedSizes;
    std::vector<GuestCharMapRec> charmaps;
    std::vector<GuestCharMapRec*> charmapPointers;
    std::vector<GuestVector> outlinePoints;

    ~GuestFace() { UnregisterBitmap_nid_no_patch(&slot.bitmap); }
};

extern "C" void RegisterBitmap_nid_no_patch(GuestFace& face);

extern "C" GuestFace* FaceOf_nid_no_patch(const GuestFaceRec* face);
extern "C" GuestFace* FaceOfSlot_nid_no_patch(const GuestGlyphSlotRec* slot);
extern "C" void SyncFace_nid_no_patch(GuestFace& face);
void SyncSize(GuestFace& face);
extern "C" void SyncSizePointer_nid_no_patch(GuestFace& face);
extern "C" void SyncSizeRecord_nid_no_patch(GuestFace& face, GuestSizeRec& rec, FT_Size host);
void SyncSlot(GuestFace& face);

}

#endif
