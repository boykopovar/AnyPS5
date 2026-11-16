# libSceLibcInternal

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) - 283 entries observed in a 3.20-labelled image and a later 13.60-labelled image; 277 match the public 3.20 catalogue and 6 do not. Apart from the implemented exports below, these remain import-resolution declarations: signatures and behavior are unverified, parameterless `int` declarations are placeholders, and their bodies throw through `NotImplemented_nid_no_patch`. All declarations use literal NID aliases; the table records candidate names where known. No other runtime functionality is implemented; data exports need separate analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- [Random-device entropy](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) (`mDIHE-aaRaI`, `std::_Random_device_entropy()`) reports positive double zero without requesting random bytes, matching five inspected title-shipped SDK modules. Zero is an unknown entropy estimate, not a zero-valued or deterministic random generator. Other SDK variants have not been checked.
