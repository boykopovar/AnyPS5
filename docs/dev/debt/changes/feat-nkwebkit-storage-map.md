# libSceNKWebKit

## add Functional

- [WebCore::StorageMap](../../../core/libs/prx/libSceNKWebKit/src/StorageMap.cpp) (libSceNKWebKit) - not compared with a console module. `key`, `getItem`, `setItem`, `setItemIgnoringQuota`, `removeItem` and `contains` follow the pinned [StorageMap.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/storage/StorageMap.cpp) on a 16-byte `StorageMap` whose 40-byte `Impl` holds a `HashMap<String, String>` laid out as in [HashTable.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/HashTable.h); both layouts are read from the source. A null key, or a size over 32 bits without a quota, throws where the source crashes. The 3.20 exports `create`, `copy` and `importItems` belong to the older reference-counted class, whose `setItem` and `removeItem` return a `RefPtr` under the same names: not modelled, so they stay stubs. Closes when: compared with a console module.

## remove | `_ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_` | `QpaYYDZ84Gk` |

## remove | `_ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_` | `yr9EzrSEsfo` |

## remove | `_ZN7WebCore10StorageMap3keyEj` | `TDRCoFObHro` |

## remove | `_ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb` | `fNAsEJUeDYQ` |

## remove | `_ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE` | `HYz9i0PIHWs` |

## remove | `_ZNK7WebCore10StorageMap8containsERKN3WTF6StringE` | `2PVKr6w9AM4` |
