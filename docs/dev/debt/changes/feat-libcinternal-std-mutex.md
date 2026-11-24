# libSceLibcInternal

## add Functional

- [`std::_Mutex` (`_ZNSt6_MutexC1Ev`, `_ZNSt6_MutexC2Ev`, `_ZNSt6_MutexD2Ev`, `_ZNSt6_Mutex5_LockEv`, `_ZNSt6_Mutex7_UnlockEv`)](../../../core/libs/prx/libSceLibcInternal/StdMutex.cpp) (libSceLibcInternal, NIDs `log6zy2C9iQ`, `j7e7EQBD6ZA`, `U54NBtdj6UY`, `7OCTkL2oWyg`) implement Dinkumware `std::_Mutex` construction, destruction, locking, and unlocking over `_Mtxinit`, `_Mtxdst`, `_Mtxlock`, and `_Mtxunlock` with guest-checked handle storage. Console behavior is unverified.

## change `_ZNSt10moneypunctIwLb0EEC2EPKcm`, `_ZNSt6_Mutex5_LockEv`

- [Additional callable exports](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) - 279 entries observed in a 3.20-labelled image and a later 13.60-labelled image; 273 match the public 3.20 catalogue and 6 do not. Apart from the `showmanyc`, empty facet, runtime hook and system_error constructor exports below, these remain import-resolution declarations: signatures and behavior are unverified, parameterless `int` declarations are placeholders, and their bodies throw through `NotImplemented_nid_no_patch`. All declarations use literal NID aliases; the table records candidate names where known. No other runtime functionality is implemented; data exports need separate analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).
