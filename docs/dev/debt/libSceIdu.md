# libSceIdu technical debt

### Unknown function info

- [sceIduGetEnv, sceIduInit, sceIduInstallPkg, sceIduIsAppInstalled, sceIduIsRifInstalled, sceIduIsStaffMode, sceIduReboot and sceIduTerm](../../../core/libs/prx/libSceIdu/Export.cpp) (libSceIdu) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceIdu.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceIdu/Export.cpp) - 13 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/752e1b1d4a6ac8a26c97e693527fefbd/a7bddd315c7740a7d065af9e80b0447fcd5b91be) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled module. The extract SHA-256 is `63256453dc9620091ce6af95f8b243b7da8f2ce76a585f4886f76aa6625b5b21`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. The extract is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior. The separately published 3.20 source catalog has a module file but none of these selected NIDs.

| Name | NID |
| --- | --- |
| `sceIduGetDiscTitleInfo` | `LgCWg-v74x0` |
| `sceIduGetFsStats` | `GtkUkPjD9Uk` |
| `sceIduGetHardwareId` | `thkjPe16qxc` |
| `sceIduGetPkgInstallStatus` | `wZNmq82rKVk` |
| `sceIduGetTitleList` | `aIki6P40SdY` |
| `sceIduInstallRif` | `GZhI4L2kqB0` |
| `sceIduNotificationHide` | `l6blKJegR98` |
| `sceIduNotificationShow` | `Dw68FfautUc` |
| `sceIduStartTitle` | `aOjpQPv29Mg` |
| `Unknown` | `3XKI6v75VEo` |
| `Unknown` | `J9y431a1uM0` |
| `Unknown` | `tmeq5nZRqIs` |
| `Unknown` | `uoHZCJW8Cdk` |
