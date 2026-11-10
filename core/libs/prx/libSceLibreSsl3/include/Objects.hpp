#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSsl3 {

inline constexpr int ObjectCount = 1056;

const LibreSslAsn1Object* ObjectByNid(int nid);
int ObjectNid(const LibreSslAsn1Object* object);
int CompareObjects(const LibreSslAsn1Object* first, const LibreSslAsn1Object* second);

} // namespace LibreSsl3
