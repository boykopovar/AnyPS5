#include "prx/libharfbuzz/include/Lifecycle.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>

#include <memory>
#include <type_traits>

static_assert(sizeof(hb_memory_mode_t) == sizeof(std::uint32_t));
static_assert(std::is_same_v<hb_tag_t, std::uint32_t>);
static_assert(HB_MEMORY_MODE_DUPLICATE == 0 && HB_MEMORY_MODE_READONLY == 1
    && HB_MEMORY_MODE_WRITABLE == 2 && HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE == 3);

namespace {

struct DestroyData {
    void* data;
    HarfBuzz::Destroy destroy;
};

struct FaceData {
    HarfBuzz::ReferenceTable referenceTable;
    void* data;
    HarfBuzz::Destroy destroy;
};

void DestroyUserData(void* opaque) {
    const std::unique_ptr<DestroyData> data(static_cast<DestroyData*>(opaque));
    data->destroy(data->data);
}

void DestroyFace(void* opaque) {
    const std::unique_ptr<FaceData> data(static_cast<FaceData*>(opaque));
    if (data->destroy) data->destroy(data->data);
}

hb_blob_t* ReferenceTable(hb_face_t* face, hb_tag_t tag, void* opaque) {
    const auto& data = *static_cast<FaceData*>(opaque);
    return data.referenceTable(face, tag, data.data);
}

}

extern "C" {

hb_blob_t* APS5_VABI hb_blob_create_nid_postfix(const char* data, unsigned length, std::uint32_t mode, void* userData, HarfBuzz::Destroy destroy) {
    if (mode > HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE || (!data && length && length < (1u << 31))) NotImplemented_nid_no_patch(__func__);
    if (!destroy) return hb_blob_create(data, length, static_cast<hb_memory_mode_t>(mode), userData, nullptr);
    auto owner = std::make_unique<DestroyData>(userData, destroy);
    return hb_blob_create(data, length, static_cast<hb_memory_mode_t>(mode), owner.release(), DestroyUserData);
}

hb_face_t* APS5_VABI hb_face_create_for_tables_nid_postfix(HarfBuzz::ReferenceTable referenceTable, void* userData, HarfBuzz::Destroy destroy) {
    if (!referenceTable) {
        if (!destroy) return hb_face_create_for_tables(nullptr, userData, nullptr);
        auto owner = std::make_unique<DestroyData>(userData, destroy);
        return hb_face_create_for_tables(nullptr, owner.release(), DestroyUserData);
    }
    auto owner = std::make_unique<FaceData>(referenceTable, userData, destroy);
    return hb_face_create_for_tables(ReferenceTable, owner.release(), DestroyFace);
}

void APS5_VABI hb_face_destroy_nid_postfix(hb_face_t* face) {
    hb_face_destroy(face);
}

}
