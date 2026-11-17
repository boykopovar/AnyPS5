# libSceIcu

## change named stubs declared in [Identified.hpp]

- The 21 parameterless throwing stubs declared in [Identified.hpp](../../../core/libs/prx/libSceIcu/Identified.hpp) carry the ICU C API names whose `tools/nid_names.py` hash equals the inventory NID; the names were taken from the identifiers of the ICU 78.1 `common`, `i18n` and `io` sources. A name match does not show that the console function has ICU's prototype or behavior. Most of them read resource, converter or character-name data that no host library provides.

## change [recovered inventory exports]

- The 134 [recovered inventory exports](../../../core/libs/prx/libSceIcu/RecoveredInventory.cpp) have unverified parameters, return types, callback ABIs and behavior. The 46 left in that file throw through `NotImplemented_nid_no_patch` as parameterless `int` placeholders; the `ubidi` and `ucsdet` ones are in [Bidi.cpp](../../../core/libs/prx/libSceIcu/Bidi.cpp) and [CharsetDetection.cpp](../../../core/libs/prx/libSceIcu/CharsetDetection.cpp). The [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/226383a7fa4b1275d6cebf61eb1c1e7ca7b58bcb/data/firmware-13.60/libsceicu-9790ee85e3.jsonl) establishes only name/NID bindings. The [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceIcu.inc) assigns all 134 NIDs to logical `libSceIcu` in `libSceIcu.sprx`. Recovered `ucfpos`, `uset` and `uiter` groups are excluded. Console ABI, behavior and title compatibility remain unverified.

## change [Break iterator factories]

- [Break iterator factories](../../../core/libs/prx/libSceIcu/Export.cpp) - Four 3.20 callable NIDs are the ICU 68 `BreakIterator::createCharacterInstance`, `createWordInstance`, `createLineInstance` and `createSentenceInstance(const Locale&, UErrorCode&)`: their `icu_68` mangled names hash to these NIDs, and the selector each wrapper passes is the `UBreakIteratorType` that ICU 68 passes to `BreakIterator::createInstance`. They are declared as throwing placeholders; their ICU 68 C++ prototypes/layouts remain unverified despite the source-built ICU 78 rule data. The four selector wrappers have identical instruction bytes in the 3.20 and later version-labelled modules, and corresponding consumer switch cases call them. Their NIDs and internal layouts differ, and the later NIDs match no `icu_60` to `icu_79` name of these functions; no forwarding or binary compatibility is claimed. Later image version metadata does not authenticate a retail firmware release.
