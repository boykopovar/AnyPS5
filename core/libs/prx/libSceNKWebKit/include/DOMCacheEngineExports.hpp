#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/DOMCacheEngineAbi.hpp"
#include "prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfHashMapStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestES3_RKNS_16ResourceResponseERKNS_17CacheQueryOptionsE(
    const NKWebKit::WebCoreAbi::ResourceRequestBase* request, const NKWebKit::WebCoreAbi::ResourceRequestBase* cachedRequest,
    const NKWebKit::WebCoreAbi::ResourceResponseBase* cachedResponse, const NKWebKit::WebCoreAbi::CacheQueryOptions* options);
bool APS5_VABI _ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestERKN3WTF3URLEbRKNS4_7HashMapINS4_6StringES9_NS4_11DefaultHashIS9_EENS4_10HashTraitsIS9_EESD_EERKNS_17CacheQueryOptionsE(
    const NKWebKit::WebCoreAbi::LegacyResourceRequestBase* request, const NKWebKit::WtfAbi::URL* url, bool hasVaryStar,
    const NKWebKit::WtfAbi::StringHashMap* varyHeaders, const NKWebKit::WebCoreAbi::CacheQueryOptions* options);

}
