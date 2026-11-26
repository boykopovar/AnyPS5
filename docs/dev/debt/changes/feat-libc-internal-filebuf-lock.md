# libSceLibcInternal

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) - 283 entries observed in a 3.20-labelled image and a later 13.60-labelled image; 277 match the public 3.20 catalogue and 6 do not. Apart from the `showmanyc`, facet, runtime hook, system_error constructor, file-buffer locking and random-device entropy exports below, these remain import-resolution declarations: signatures and behavior are unverified, parameterless `int` declarations are placeholders, and their bodies throw through `NotImplemented_nid_no_patch`. The remaining placeholders use literal NID aliases; the table records candidate names where known. Other data exports remain unverified. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- [basic_filebuf<char>::_Lock and basic_filebuf<wchar_t>::_Lock](../../../core/libs/prx/libSceLibcInternal/Filebuf.cpp) are not measured on a console or verified beyond SDK 10.00. SDK 10.00 `libc.prx` at `0x84560` and `0xb4ae0` reads the file pointer at object offset `0xa8`, returns for a null file, and otherwise calls `_Lockfilelock` at `0x35f0`; `target/include/fstream:304-307` gives the void member signature. The guest file backend provides the recursive lock. Unreadable or wrapping object addresses throw instead of reproducing the SDK native memory fault.
