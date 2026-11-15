# libSceIcu

## change named stubs declared in [Identified.hpp]

- The 21 parameterless throwing stubs declared in [Identified.hpp](../../../core/libs/prx/libSceIcu/Identified.hpp) carry the ICU C API names whose `tools/nid_names.py` hash equals the inventory NID; the names were taken from the identifiers of the ICU 78.1 `common`, `i18n` and `io` sources. A name match does not show that the console function has ICU's prototype or behavior. Most of them read resource, converter or character-name data that no host library provides.

## change [recovered inventory exports]

- The 134 [recovered inventory exports](../../../core/libs/prx/libSceIcu/RecoveredInventory.cpp) have unverified parameters, return types, callback ABIs and behavior. The 43 left in that file throw through `NotImplemented_nid_no_patch` as parameterless `int` placeholders; the `ubidi` and `ucsdet` ones are in [Bidi.cpp](../../../core/libs/prx/libSceIcu/Bidi.cpp) and [CharsetDetection.cpp](../../../core/libs/prx/libSceIcu/CharsetDetection.cpp). The [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/226383a7fa4b1275d6cebf61eb1c1e7ca7b58bcb/data/firmware-13.60/libsceicu-9790ee85e3.jsonl) establishes only name/NID bindings. The [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceIcu.inc) assigns all 134 NIDs to logical `libSceIcu` in `libSceIcu.sprx`. Recovered `ucfpos`, `uset` and `uiter` groups are excluded. Console ABI, behavior and title compatibility remain unverified.

## change [Break iterator factories]

- [Break iterator factories](../../../core/libs/prx/libSceIcu/Export.cpp) - Four 3.20 callable NIDs are the ICU 68 `BreakIterator::createCharacterInstance`, `createWordInstance`, `createLineInstance` and `createSentenceInstance(const Locale&, UErrorCode&)`: their `icu_68` mangled names hash to these NIDs, and the selector each wrapper passes is the `UBreakIteratorType` that ICU 68 passes to `BreakIterator::createInstance`. They are declared as throwing placeholders; their ICU 68 C++ prototypes/layouts remain unverified despite the source-built ICU 78 rule data. The four selector wrappers have identical instruction bytes in the 3.20 and later version-labelled modules, and corresponding consumer switch cases call them. Their NIDs and internal layouts differ, and the later NIDs match no `icu_60` to `icu_79` name of these functions; no forwarding or binary compatibility is claimed. Later image version metadata does not authenticate a retail firmware release.

## change [`u_getIntPropertyMap`]

- [`u_getIntPropertyMap`](../../../core/libs/prx/libSceIcu/Properties.cpp) forwards integer-property map lookups to bundled ICU 78.1; its NID name comes from the recovered inventory, while the prototype follows ICU's public header. Its existing guards for maps backed by ICU data files remain, even with the source-built package. The map uses Unicode 17.0 property data and has not been checked on a console.

## change [Locale writing direction]

- [Locale writing direction](../../../core/libs/prx/libSceIcu/RightToLeft.cpp) is unverified on a console. `uloc_isRightToLeft` throws for a null locale (ICU uses the default locale) and for a locale with no script whose language is not one of the 17 that ICU 78.1 [loclikely.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/loclikely.cpp#L348-L386) lists: the existing likely-subtags guard remains even with the source-built data package. Otherwise it follows that function, with the script direction from the ICU submodule. The NID is the ICU 68 name; [ICU 68.2](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/loclikely.cpp#L1301-L1348) returns false for a language of eight or more letters without a script, which throws here, and for the scripts `Ougr`, `Gara` and `Sidt`, which return true here. Closes when: compared with the console and the likely subtags data is available.
