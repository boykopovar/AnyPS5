#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_OBJECTTABLE_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_OBJECTTABLE_HPP

#include "prx/libSceLibreSsl/include/Api.hpp"

inline constexpr int LibreSslObjectCount = 956;
inline constexpr int LibreSslObjectOrderCount = 890;

extern "C" {

extern const LibreSslAsn1Object LibreSslObjects_nid_no_patch[LibreSslObjectCount];
extern const unsigned short LibreSslObjectOrder_nid_no_patch[LibreSslObjectOrderCount];

int LibreSslObjectNid_nid_no_patch(const LibreSslAsn1Object* object);
const char* LibreSslObjectShortName_nid_no_patch(int nid);
const char* LibreSslObjectLongName_nid_no_patch(int nid);
int LibreSslAddedObjectNid_nid_no_patch(const LibreSslAsn1Object* object);
const char* LibreSslAddedObjectName_nid_no_patch(int nid, bool longName);

}

#endif
