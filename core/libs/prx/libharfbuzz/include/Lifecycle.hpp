#pragma once

#include "prx/libharfbuzz/include/FontFunctions.hpp"

struct hb_blob_t;

namespace HarfBuzz {

using ReferenceTable = hb_blob_t* (APS5_VABI *)(hb_face_t*, std::uint32_t, void*);

}

extern "C" {

hb_blob_t* APS5_VABI hb_blob_create_nid_postfix(const char* data, unsigned length, std::uint32_t mode, void* userData, HarfBuzz::Destroy destroy);
hb_face_t* APS5_VABI hb_face_create_for_tables_nid_postfix(HarfBuzz::ReferenceTable referenceTable, void* userData, HarfBuzz::Destroy destroy);
void APS5_VABI hb_face_destroy_nid_postfix(hb_face_t* face);

}
