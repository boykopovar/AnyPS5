# libSceNKWebKit

## change [WebCore::ResourceRequestBase]

- [WebCore::ResourceRequestBase](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - the layout in `ResourceRequestAbi.hpp` comes from [ResourceRequestBase.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/ResourceRequestBase.h) and is not checked against the PS5 module. Every member accessor except `hasHTTPHeaderField` and `setCachePartition` throws when `m_resourceRequestUpdated` is false, unless it returns on a no-op path (`removeCredentials`, `setExistingHTTPReferrerToOriginString`), because the PS5 `doUpdateResourceRequest` is unknown, and `httpBody`, `hasUpload` and `equal` also when `m_resourceRequestBodyUpdated` is false. `setHTTPReferrer` throws for a referrer over 4096 code units, which the source rewrites through `SecurityOrigin`; `redirectedRequest` still throws `NotImplemented_nid_no_patch`. Closes when: compared with a console module.

## add Functional

- [WebCore::ResourceRequestBase cache partition](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - `setCachePartition` is a no-op and `partitionName` returns the shared empty string for every domain, neither touching its arguments, assuming `ENABLE(CACHE_PARTITIONING)` and `ENABLE(PUBLIC_SUFFIX_LIST)` are 0 as everywhere outside [PlatformEnableCocoa.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PlatformEnableCocoa.h); the console values are unverified; no export reads `m_cachePartition`. Closes when: compared with a console module.
- [WebCore::ResourceRequestBase credentials](../../../core/libs/prx/libSceNKWebKit/src/ResourceRequest.cpp) (libSceNKWebKit) - `removeCredentials` throws when the URL is valid and has credentials (`passwordEnd > userStart`), because the console re-parses through `URL::remove` and `parse`, which are not implemented; an invalid URL with credentials takes the console no-op. `setExistingHTTPReferrerToOriginString` throws when a referrer exists, because `SecurityPolicy::referrerToOriginString` needs `SecurityOrigin::createFromString` and `toString`. Closes when: compared with a console module.

## remove | `_ZN7WebCore19ResourceRequestBase12clearPurposeEv` | `gwVFNCkuOQk` |

## remove | `_ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE` | `1ULaJ4WPLtI` |

## remove | `_ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv` | `ZjGWQIfLDE4` |

## remove | `_ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE` | `bXhOOnwplgg` |

## remove | `_ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE` | `vLOl08NCj+0` |

## remove | `_ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE` | `cJokY6wAIvA` |

## remove | `_ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv` | `3hl+3pWsRNY` |

## remove | `_ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_` | `XMPmjIgxBPc` |

## remove | `_ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_` | `-swGGmA0SP8` |

## remove | `_ZNK7WebCore19ResourceRequestBase12isolatedCopyEv` | `FnhAa5JKqhE` |

## remove | `_ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv` | `3Yr1O6r-8ms` |

## remove | `_ZNK7WebCore19ResourceRequestBase8httpBodyEv` | `pLh4waaqW7c` |

## remove | `_ZNK7WebCore19ResourceRequestBase9hasUploadEv` | `9aGME3ZvhCY` |
