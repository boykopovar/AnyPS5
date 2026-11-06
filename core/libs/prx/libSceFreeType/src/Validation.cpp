#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_GX_VALIDATE_H
#include FT_OPENTYPE_VALIDATE_H

using namespace FreeType;

extern "C" {

int APS5_VABI FT_OpenType_Validate_nid_postfix(GuestFaceRec* face, std::uint32_t flags, const unsigned char** base, const unsigned char** gdef, const unsigned char** gpos, const unsigned char** gsub, const unsigned char** jstf) {
    const GuestFace* owner = FaceOf(face);
    return FT_OpenType_Validate(owner ? owner->host : nullptr, flags, base, gdef, gpos, gsub, jstf);
}

void APS5_VABI FT_OpenType_Free_nid_postfix(GuestFaceRec* face, const unsigned char* table) {
    const GuestFace* owner = FaceOf(face);
    FT_OpenType_Free(owner ? owner->host : nullptr, table);
}

int APS5_VABI FT_TrueTypeGX_Validate_nid_postfix(GuestFaceRec* face, std::uint32_t flags, const unsigned char** tables, std::uint32_t tableCount) {
    const GuestFace* owner = FaceOf(face);
    return FT_TrueTypeGX_Validate(owner ? owner->host : nullptr, flags, tables, tableCount);
}

void APS5_VABI FT_TrueTypeGX_Free_nid_postfix(GuestFaceRec* face, const unsigned char* table) {
    const GuestFace* owner = FaceOf(face);
    FT_TrueTypeGX_Free(owner ? owner->host : nullptr, table);
}

int APS5_VABI FT_ClassicKern_Validate_nid_postfix(GuestFaceRec* face, std::uint32_t flags, const unsigned char** table) {
    const GuestFace* owner = FaceOf(face);
    return FT_ClassicKern_Validate(owner ? owner->host : nullptr, flags, table);
}

void APS5_VABI FT_ClassicKern_Free_nid_postfix(GuestFaceRec* face, const unsigned char* table) {
    const GuestFace* owner = FaceOf(face);
    FT_ClassicKern_Free(owner ? owner->host : nullptr, table);
}

}
