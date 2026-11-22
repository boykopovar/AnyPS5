# libSceIcu

## add Functional

- [uloc_openKeywordList](../../../core/libs/prx/libSceIcu/KeywordList.cpp) is not run on a console. It opens the bundled ICU 78.1 [`uloc_openKeywordList`](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc.cpp#L1712-L1737), whose body and keyword enumeration are the same in [ICU 68.2](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/uloc.cpp#L1367-L1393): it copies `keywordListSize` bytes, appends a NUL and lists the NUL-separated keywords up to the first empty one. A negative size and `INT32_MAX`, for which ICU's `keywordListSize+1` allocation and copy are undefined, a nonempty list whose last byte is not NUL, whose enumeration ICU reads past its copy, and an unreadable list throw `std::invalid_argument`. Closes when: compared with the console.
