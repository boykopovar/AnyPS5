# libSceNKWebKit

## add Functional

- [NULL items in WK arrays and dictionaries](../../../core/libs/prx/libSceNKWebKit/src/Array.cpp) (libSceNKWebKit) - not checked on a console. `WKArrayCreate`, `WKArrayCreateAdoptingValues`, `WKArrayAppendItem` and `WKDictionarySetItem` store a NULL item as [WKArray.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/WKArray.cpp#L37-L57), `WKMutableArray.cpp` and `WKMutableDictionary.cpp` do: `toImpl(NULL)` is not dereferenced, the item counts in the size and reads back as NULL. Two neighbours still differ: `WKDictionaryCreate` throws on a NULL value where WebKit stores it, and `WKContextConfigurationSetOverrideLanguages` skips a NULL element where `API::Array::toStringVector` dereferences it. Closes when: both follow the pinned source.
