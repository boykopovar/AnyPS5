# libSceIcu

## add Functional

- [Break iterator guest memory and handle lifetime](../../../core/libs/prx/libSceIcu/BreakIterator.cpp) and [common data guest memory](../../../core/libs/prx/libSceIcu/Data.cpp) still forward guest pointers to native ICU. A non-null unmapped or nonwritable status pointer, unreadable locale/text/data storage, or a forged, freed or concurrently closed non-null break iterator can fault the host; null checks do not establish memory access or handle ownership. The checked copies and handle registry in Idna.cpp do not protect these inherited wrappers. Console behavior for these inputs is unmeasured. Closes when: checked guest access and iterator lifetime ownership reject these inputs, with guarded-page and stale/forged-handle regressions on the supported host platforms.

## change named stubs declared in [Identified.hpp]

- The 21 parameterless throwing stubs declared in [Identified.hpp](../../../core/libs/prx/libSceIcu/Identified.hpp) carry the ICU C API names whose `tools/nid_names.py` hash equals the inventory NID; the names were taken from the identifiers of the ICU 78.1 `common`, `i18n` and `io` sources. A name match does not show that the console function has ICU's prototype or behavior. Most of them read resource, converter or character-name data that no host library provides.

## change [Break iterator factories]

- [Break iterator factories](../../../core/libs/prx/libSceIcu/Export.cpp) - Four 3.20 callable NIDs are the ICU 68 `BreakIterator::createCharacterInstance`, `createWordInstance`, `createLineInstance` and `createSentenceInstance(const Locale&, UErrorCode&)`: their `icu_68` mangled names hash to these NIDs, and the selector each wrapper passes is the `UBreakIteratorType` that ICU 68 passes to `BreakIterator::createInstance`. They are declared as throwing placeholders; their ICU 68 C++ prototypes/layouts remain unverified despite the source-built ICU 78 rule data. The four selector wrappers have identical instruction bytes in the 3.20 and later version-labelled modules, and corresponding consumer switch cases call them. Their NIDs and internal layouts differ, and the later NIDs match no `icu_60` to `icu_79` name of these functions; no forwarding or binary compatibility is claimed. Later image version metadata does not authenticate a retail firmware release.
