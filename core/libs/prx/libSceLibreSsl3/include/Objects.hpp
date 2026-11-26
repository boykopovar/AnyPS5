#pragma once

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSsl3 {

inline constexpr int ObjectCount = 1056;

const LibreSslAsn1Object& ObjectAt(int nid);
const LibreSslAsn1Object* ObjectByNid(int nid);
Asn1Object_nid_no_patch* ObjectFromText(const char* text, int noName);
int ObjectNid(const LibreSslAsn1Object* object);
int CompareObjects(const LibreSslAsn1Object* first, const LibreSslAsn1Object* second);

} // namespace LibreSsl3
