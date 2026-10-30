# libSceLibcInternal technical debt

### Additional callable exports

The named entries below use provider-specific NID aliases to avoid interposing host libc and C++ runtime symbols.

- The [revision-pinned identity inventory](https://gist.github.com/kyon-eth/6bd58c3d7a0fa933dd4b524754c43265/e4e5dbc43890a24e35dc1c656135000210e28390) records module, library, symbol type and NIDs for [InventoryExports.cpp](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp); it is a metadata summary rather than independent verification. One entry belongs to the logical library `libSceLibcInternalExt` within this module.

- [Additional callable exports](../../../core/libs/prx/libSceLibcInternal/InventoryExports.cpp) - 297 entries observed in a 3.20-labelled image and a later 13.60-labelled image; 291 match the public 3.20 catalogue and 6 do not. These are declarations only; neither image label authenticates a retail firmware version. Signatures and behavior are unverified; parameterless `int` declarations are placeholders. Every entry throws through `NotImplemented_nid_no_patch`. All declarations use literal NID aliases; the table records candidate names where known. No runtime functionality is implemented; data exports need separate analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `_ZNKSt22_System_error_category23default_error_conditionEi` | `ww3UUl317Ng` |
| `_ZNKSt22_System_error_category4nameEv` | `dXy+lFOiaQA` |
| `_ZNKSt22_System_error_category7messageEi` | `HpAlvhNKb2Y` |
| `_ZNSt10moneypunctIwLb0EEC2EPKcm` | `5bQqdR3hTZw` |
| `_ZNSt10moneypunctIwLb0EEC2ERKSt8_Locinfomb` | `1kvQkOSaaVo` |
| `_ZNSt10moneypunctIwLb0EEC2Em` | `bBGvmspg3Xs` |
| `_ZNSt10moneypunctIwLb0EED0Ev` | `95MaQlRbfC8` |
| `_ZNSt10moneypunctIwLb0EED1Ev` | `ki5SQPsB4m4` |
| `_ZNSt10moneypunctIwLb0EED2Ev` | `6F1JfiING18` |
| `_ZNSt10moneypunctIwLb1EE7_GetcatEPPKNSt6locale5facetEPKS1_` | `HmcMLz3cPkM` |
| `_ZNSt10moneypunctIwLb1EEC1EPKcm` | `pyBabUesXN4` |
| `_ZNSt10moneypunctIwLb1EEC1ERKSt8_Locinfomb` | `uROsAczW6OU` |
| `_ZNSt10moneypunctIwLb1EEC1Em` | `X69UlAXF-Oc` |
| `_ZNSt10moneypunctIwLb1EEC2EPKcm` | `GS1AvxBwVgY` |
| `_ZNSt10moneypunctIwLb1EEC2Em` | `sTaUDDnGOpE` |
| `_ZNSt10moneypunctIwLb1EED1Ev` | `OM0FnA7Tldk` |
| `_ZNSt10moneypunctIwLb1EED2Ev` | `uVOxy7dQTFc` |
| `_ZNSt12future_errorD0Ev` | `rNYLEsL7M0k` |
| `_ZNSt12future_errorD2Ev` | `XFh0C66aEms` |
| `_ZNSt12system_errorC2ESt10error_codePKc` | `oly3wSwEJ2A` |
| `_ZNSt13bad_exceptionD0Ev` | `lX9M5u0e48k` |
| `_ZNSt13bad_exceptionD1Ev` | `t6egllDqQ2M` |
| `_ZNSt13bad_exceptionD2Ev` | `iWNC2tkDgxw` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE4syncEv` | `VNaqectsZNs` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE5_LockEv` | `9biiDDejX3Q` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE5uflowEv` | `jfr41GGp2jA` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE6setbufEPci` | `SYFNsz9K2rs` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE7seekoffElNSt5_IosbIiE8_SeekdirENS4_9_OpenmodeE` | `7oio2Gs1GNk` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE7seekposESt4fposI9_MbstatetENSt5_IosbIiE9_OpenmodeE` | `rsS5cBMihAM` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEE8overflowEi` | `oYMRgkQHoJM` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEED0Ev` | `MYCRRmc7cDA` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEED1Ev` | `Yc2gZRtDeNQ` |
| `_ZNSt13basic_filebufIcSt11char_traitsIcEED2Ev` | `gOxGOQmSVU0` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEE5_LockEv` | `GYTma8zq0NU` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEE6setbufEPwi` | `wAcnCK2HCeI` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEE7seekposESt4fposI9_MbstatetENSt5_IosbIiE9_OpenmodeE` | `10VcrHqHAlw` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEE8overflowEi` | `PjH5dZGfQHQ` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEE9pbackfailEi` | `NeiFvKblpZM` |
| `_ZNSt13basic_filebufIwSt11char_traitsIwEED0Ev` | `JJ-mkOhdook` |
| `_ZNSt13basic_istreamIwSt11char_traitsIwEED1Ev` | `RP7ijkGGx50` |
| `_ZNSt13basic_ostreamIwSt11char_traitsIwEE6sentryC2ERS2_` | `4GbIwW5u5us` |
| `_ZNSt13basic_ostreamIwSt11char_traitsIwEE6sentryD2Ev` | `MB1VCygerRU` |
| `_ZNSt13basic_ostreamIwSt11char_traitsIwEED0Ev` | `7VRfkz22vPk` |
| `_ZNSt13basic_ostreamIwSt11char_traitsIwEED1Ev` | `EYZJsnX58DE` |
| `_ZNSt14overflow_errorD2Ev` | `OypvNf3Uq3c` |
| `_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci` | `VpwymQiS4ck` |
| `_ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci` | `sXaxl1QGorg` |
| `_ZNSt15basic_streambufIcSt11char_traitsIcEE9showmanycEv` | `IAEl1Ta7yVU` |
| `_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi` | `lZVehk7yFok` |
| `_ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi` | `041c37QfoUc` |
| `_ZNSt15basic_streambufIwSt11char_traitsIwEE9showmanycEv` | `olsoiZsezkk` |
| `_ZNSt15underflow_errorD2Ev` | `xiqd+QkuYXc` |
| `_ZNSt16nested_exceptionD1Ev` | `+qKS53qzWdA` |
| `_ZNSt16nested_exceptionD2Ev` | `8R00hgzXQDY` |
| `_ZNSt22_Future_error_categoryD0Ev` | `WDKzMM-uuLE` |
| `_ZNSt22_System_error_categoryD0Ev` | `Dc4ZMWmPMl8` |
| `_ZNSt22_System_error_categoryD1Ev` | `hVQgfGhJz3U` |
| `_ZNSt23_Generic_error_categoryD1Ev` | `MAalgQhejPc` |
| `_ZNSt24_Iostream_error_categoryD1Ev` | `27Z-Cx1jbkU` |
| `_ZNSt3pmr19new_delete_resourceEv` | `9G32u5RRYxE` |
| `_ZNSt3pmr20get_default_resourceEv` | `d2u38zs4Pe8` |
| `_ZNSt4_Pad7_LaunchEPKcPP12pthread_attrPP7pthread` | `H7-7Z3ixv-w` |
| `_ZNSt4_Pad7_LaunchEPKcPP7pthread` | `PBbZjsL6nfc` |
| `_ZNSt4_PadC2EPKc` | `uhnb6dnXOnc` |
| `_ZNSt5ctypeIcED0Ev` | `p8-44cVeO84` |
| `_ZNSt5ctypeIcED1Ev` | `tPsGA6EzNKA` |
| `_ZNSt5ctypeIwED0Ev` | `zOPA5qnbW2U` |
| `_ZNSt6_Mutex5_LockEv` | `U54NBtdj6UY` |
| `_ZNSt6_Mutex7_UnlockEv` | `7OCTkL2oWyg` |
| `_ZNSt6_MutexC2Ev` | `log6zy2C9iQ` |
| `_ZNSt6_MutexD2Ev` | `j7e7EQBD6ZA` |
| `_ZNSt6_WinitC1Ev` | `-Bl9-SZ2noc` |
| `_ZNSt6_WinitC2Ev` | `57mMrw0l-40` |
| `_ZNSt6locale16_SetgloballocaleEPv` | `1TaQLyPDJEY` |
| `_ZNSt6locale5emptyEv` | `K-5mtupQZ4g` |
| `_ZNSt6locale5facet7_DecrefEv` | `AgxEl+HeWRQ` |
| `_ZNSt6locale5facet7_IncrefEv` | `-EgSegeAKl4` |
| `_ZNSt6locale7_Locimp8_MakelocERKSt8_LocinfoiPS0_PKS_` | `5r801ZWiJJI` |
| `_ZNSt6locale7_Locimp9_MakewlocERKSt8_LocinfoiPS0_PKS_` | `BcbHFSrcg3Y` |
| `_ZNSt6locale7_Locimp9_MakexlocERKSt8_LocinfoiPS0_PKS_` | `fkFGlPdquqI` |
| `_ZNSt6locale7_LocimpC2ERKS0_` | `dsJKehuajH4` |
| `_ZNSt6locale7_LocimpD1Ev` | `aD-iqbVlHmQ` |
| `_ZNSt6locale7_LocimpD2Ev` | `So6gSmJMYDs` |
| `_ZNSt6localeD1Ev` | `pMWnITHysPc` |
| `_ZNSt7_MpunctIcE5_InitERKSt8_Locinfob` | `m7qAgircaZY` |
| `_ZNSt7_MpunctIcEC2EPKcmbb` | `0il9qdo6fhs` |
| `_ZNSt7_MpunctIcED1Ev` | `0AeC+qCELEA` |
| `_ZNSt7_MpunctIwE5_InitERKSt8_Locinfob` | `iCoD0EOIbTM` |
| `_ZNSt7_MpunctIwEC2EPKcmbb` | `TDhjx3nyaoU` |
| `_ZNSt7_MpunctIwED1Ev` | `0TADyPWrobI` |
| `_ZNSt7codecvtIDic9_MbstatetED0Ev` | `vwMx2NhWdLw` |
| `_ZNSt7codecvtIDic9_MbstatetED1Ev` | `TuhGCIxgLvA` |
| `_ZNSt7codecvtIDsc9_MbstatetED0Ev` | `zYHryd8vd0w` |
| `_ZNSt7codecvtIcc9_MbstatetEC1ERKSt8_Locinfom` | `3YCLxZqgIdo` |
| `_ZNSt7codecvtIcc9_MbstatetEC2ERKSt8_Locinfom` | `4qHwSTPt-t8` |
| `_ZNSt7codecvtIcc9_MbstatetEC2Em` | `e5Hwcntvd8c` |
| `_ZNSt7codecvtIcc9_MbstatetED0Ev` | `2TYdayAO39E` |
| `_ZNSt7codecvtIcc9_MbstatetED1Ev` | `djNkrJKTb6Q` |
| `_ZNSt7codecvtIwc9_MbstatetED1Ev` | `0fkFA3za2N8` |
| `_ZNSt7collateIcEC2ERKSt8_Locinfom` | `L71JAnoQees` |
| `_ZNSt7collateIcEC2Em` | `tkqNipin1EI` |
| `_ZNSt7collateIcED0Ev` | `Lt4407UMs2o` |
| `_ZNSt7collateIcED1Ev` | `8pXCeme0FC4` |
| `_ZNSt7collateIcED2Ev` | `dP5zwQ2Yc8g` |
| `_ZNSt7collateIwE5_InitERKSt8_Locinfo` | `LxKs-IGDsFU` |
| `_ZNSt7collateIwEC1EPKcm` | `fjHAU8OSaW8` |
| `_ZNSt7collateIwEC1ERKSt8_Locinfom` | `wggIIjWSt-E` |
| `_ZNSt7collateIwEC2EPKcm` | `PSAw7g1DD24` |
| `_ZNSt7collateIwEC2ERKSt8_Locinfom` | `2PoQu-K2qXk` |
| `_ZNSt7collateIwEC2Em` | `HQbgeUdQyyw` |
| `_ZNSt7collateIwED0Ev` | `ig4VDIRc21Q` |
| `_ZNSt7collateIwED1Ev` | `ZO3a6HfALTQ` |
| `_ZNSt7collateIwED2Ev` | `84wIPnwBGiU` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em` | `uXj-oWD2334` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom` | `yAobGI2Whrg` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em` | `RNmYVYlZvv8` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `-1G1iE3KyGI` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev` | `kAay0hfgDJs` |
| `_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev` | `6S8jzWWGcWo` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo` | `4MHgRGOKOXY` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom` | `0UPU3kvxWb0` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em` | `18rLbEV-NQs` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom` | `e3shgCIZxRc` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em` | `-+RKa3As0gE` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev` | `aHDdLa7jA1U` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev` | `Zbaaq-d70ms` |
| `_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev` | `bwVJf3kat9c` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo` | `BbJ4naWZeRw` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom` | `Hg1im-rUeHc` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em` | `LAEVU8cBSh4` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom` | `Mniutm2JL2M` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `aOK5ucXO-5g` |
| `_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev` | `WoCt9o2SYHw` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom` | `Ky+C-qbKcX0` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em` | `xdHqQoggdfo` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em` | `f1ZGLUnQGgo` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev` | `jyXTVnmlJD4` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev` | `WiUy3dEtCOQ` |
| `_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev` | `6hV3y21d59k` |
| `_ZNSt8_Locinfo8_AddcatsEiPKc` | `WkAsdy5CUAo` |
| `_ZNSt8_LocinfoC1ERKSs` | `2aSk2ruCP0E` |
| `_ZNSt8_LocinfoC2EPKc` | `pN02FS5SPgg` |
| `_ZNSt8_LocinfoC2ERKSs` | `ReK9U6EUWuU` |
| `_ZNSt8_LocinfoC2EiPKc` | `i180MNC9p4c` |
| `_ZNSt8ios_base4InitC1Ev` | `sqWytnhYdEg` |
| `_ZNSt8ios_base4InitD1Ev` | `kxXCvcat1cM` |
| `_ZNSt8ios_base4InitD2Ev` | `bxLH5WHgMBY` |
| `_ZNSt8ios_base5clearENSt5_IosbIiE8_IostateEb` | `QXJCcrXoqpU` |
| `_ZNSt8ios_baseD0Ev` | `I5jcbATyIWo` |
| `_ZNSt8messagesIcE5_InitERKSt8_Locinfo` | `eLB2+1+mVvg` |
| `_ZNSt8messagesIcE7_GetcatEPPKNSt6locale5facetEPKS1_` | `96Ev+CE1luE` |
| `_ZNSt8messagesIcEC1Em` | `6gCBQs1mIi4` |
| `_ZNSt8messagesIcEC2EPKcm` | `qF3mHeMAHVk` |
| `_ZNSt8messagesIcED1Ev` | `34mi8lteNTs` |
| `_ZNSt8messagesIcED2Ev` | `yDdbQr1oLOc` |
| `_ZNSt8messagesIwE7_GetcatEPPKNSt6locale5facetEPKS1_` | `XghI4vmw8mU` |
| `_ZNSt8messagesIwEC1Em` | `n4+3hznhkU4` |
| `_ZNSt8messagesIwEC2EPKcm` | `DlDsyX+XsoA` |
| `_ZNSt8messagesIwEC2ERKSt8_Locinfom` | `DDQjbwNC31E` |
| `_ZNSt8messagesIwED1Ev` | `6sAaleB7Zgk` |
| `_ZNSt8messagesIwED2Ev` | `I-e7Dxo087A` |
| `_ZNSt8numpunctIcE5_InitERKSt8_Locinfob` | `1LvbNeZZJ-o` |
| `_ZNSt8numpunctIcE5_TidyEv` | `fFnht9SPed8` |
| `_ZNSt8numpunctIcEC1ERKSt8_Locinfomb` | `CDm+TUClE7E` |
| `_ZNSt8numpunctIcEC1Em` | `TEtyeXjcZ0w` |
| `_ZNSt8numpunctIcEC2EPKcmb` | `yIn4l8OO1zA` |
| `_ZNSt8numpunctIcEC2Em` | `1eVdDzPtzD4` |
| `_ZNSt8numpunctIcED1Ev` | `qEob3o53s2M` |
| `_ZNSt8numpunctIwE5_TidyEv` | `2YCDWkuFEy8` |
| `_ZNSt8numpunctIwEC1EPKcmb` | `H+T2VJ91dds` |
| `_ZNSt8numpunctIwEC1ERKSt8_Locinfomb` | `s1EM2NdPf0Y` |
| `_ZNSt8numpunctIwEC1Em` | `XOgjMgZ3fjo` |
| `_ZNSt8numpunctIwEC2EPKcmb` | `m4kEqv7eGVY` |
| `_ZNSt8numpunctIwEC2ERKSt8_Locinfomb` | `MQJQCxbLfM0` |
| `_ZNSt8numpunctIwEC2Em` | `ElKI+ReiehU` |
| `_ZNSt8numpunctIwED0Ev` | `VHBnRBxBg5E` |
| `_ZNSt8numpunctIwED1Ev` | `lzK3uL1rWJY` |
| `_ZNSt8numpunctIwED2Ev` | `XDm4jTtoEbo` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_TidyEv` | `+yrOX7MgVlk` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom` | `iO5AOflrTaA` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2EPKcm` | `M0n7l76UVyE` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom` | `l7OtvplI42U` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em` | `dU8Q2yzFNQg` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `mmq9OwwYx74` |
| `_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev` | `Cp9ksNOeun8` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_TidyEv` | `ImblNB7fVVU` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_` | `e5jQyuEE+9U` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom` | `y0hzUSFrkng` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em` | `J2xO4cttypo` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom` | `bU3S1OS1sc0` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em` | `p-SW25yE-Q8` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev` | `8H3yBUytv-0` |
| `_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev` | `QTgRx1NTp6o` |
| `_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo` | `QdPT7uDTlo0` |
| `_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em` | `6htjEH2Gi-w` |
| `_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `WQQlL0n2SpU` |
| `_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_` | `bF2uVCqVhBY` |
| `_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom` | `oi3kpQPqpMY` |
| `_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom` | `b1LciG4lUUk` |
| `_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em` | `lOF5jrFNZUA` |
| `_ZNSt9basic_iosIcSt11char_traitsIcEE4initEPSt15basic_streambufIcS1_Eb` | `UQPicLg8Sx8` |
| `_ZNSt9basic_iosIwSt11char_traitsIwEE4initEPSt15basic_streambufIwS1_Eb` | `uqLGWOX7-YE` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo` | `6zg3ziZ4Qis` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5_` | `MSSvHmcbs3g` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom` | `7NQGsY7VY3c` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em` | `YGPopdkhujM` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom` | `iWtXRduTjHA` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC2Em` | `f+1EaDVL5C4` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `b9QSruV4nnc` |
| `_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev` | `CClObiVHzDY` |
| `_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo` | `JOj6qfc4VLs` |
| `_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom` | `ej+44l1PjjY` |
| `_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev` | `RB3ratfpZDc` |
| `_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED1Ev` | `a6yvHMSqsV0` |
| `_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev` | `7ZmeGHyM6ew` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIcEEEm` | `66AuqgLnsQE` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE4_RepES3_cm` | `1dY2KJfkgMM` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo` | `riBxNiKLvI0` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5_` | `w9fCz0pbHdw` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom` | `mdYczJb+bb0` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em` | `Qi5fpNt5+T0` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom` | `b2na0Dzd5j8` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em` | `XqbpfYmAZB4` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev` | `s2zG12AYKTg` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev` | `AnE9WWbyWkM` |
| `_ZNSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev` | `MuACiCSA8-s` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm` | `6QU40olMkOM` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo` | `kJmdxo4uM+8` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_` | `0sHarDG9BY4` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em` | `rme+Po9yI5M` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2Em` | `jIvWFH24Bjw` |
| `_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev` | `4+oswXtp7PQ` |
| `_ZNSt9type_infoD0Ev` | `N5nZ3wQw+Vc` |
| `_ZNSt9type_infoD1Ev` | `LLssoYjMOow` |
| `_ZNSt9type_infoD2Ev` | `zb3436295XU` |
| `_ZSt10_GetloctxtIcSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_` | `FX+eS2YsEtY` |
| `_ZSt10_GetloctxtIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT_` | `i4J5FvRPG-w` |
| `_ZSt10_Rng_abortPKc` | `VrWUXy1gqn8` |
| `_ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE` | `qMXslRdZVj4` |
| `_ZSt14_Debug_messagePKcS0_j` | `Zn44KZgJtWY` |
| `_ZSt14get_unexpectedv` | `zPWCqkg7V+o` |
| `_ZSt14set_unexpectedPFvvE` | `Eva2i4D5J6I` |
| `_ZSt16_Xoverflow_errorPKc` | `saSUnPWLq9E` |
| `_ZSt17_Future_error_mapi` | `0r8rbw2SFqk` |
| `_ZSt22_Random_device_entropyv` | `mDIHE-aaRaI` |
| `_ZSt6_ThrowRKSt9exception` | `pDFe-IgbTPg` |
| `_ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi` | `aqyjhIx7jaY` |
| `_ZSt7_MP_AddPyy` | `QfPuSqhub7o` |
| `_ZSt7_MP_GetPy` | `lrQSsTRMMr4` |
| `_ZSt7_MP_MulPyyy` | `Gav1Xt1Ce+c` |
| `_ZSt7_MP_RemPyy` | `Ozk+Z6QnlTY` |
| `_ZSt7setbasei` | `4rrOHCHAV1w` |
| `_ZSt9_LStrxfrmIcEmPT_S1_PKS0_S3_PKSt8_Collvec` | `V62E2Q8bJVY` |
| `_ZSt9_LStrxfrmIwEmPT_S1_PKS0_S3_PKSt8_Collvec` | `BloPUt1HCH0` |
| `_ZTv0_n24_NSiD1Ev` | `+Uuj++A+I14` |
| `_ZTv0_n24_NSoD0Ev` | `QJJ-4Dgm8YQ` |
| `_ZTv0_n24_NSoD1Ev` | `kvqg376KsJo` |
| `_ZTv0_n24_NSt13basic_istreamIwSt11char_traitsIwEED1Ev` | `q05IXuNA2NE` |
| `_ZTv0_n24_NSt13basic_ostreamIwSt11char_traitsIwEED0Ev` | `0j1jspKbuFk` |
| `_ZTv0_n24_NSt13basic_ostreamIwSt11char_traitsIwEED1Ev` | `HSkPyRyFFHQ` |
| `_ZdaPvS_` | `Suc8W0QPxjw` |
| `_ZdlPvS_` | `1vo6qqqa9F4` |
| `__atomic_compare_exchange_n` | `hsn2TaF3poY` |
| `__atomic_exchange_n` | `oCH4efUlxZ0` |
| `__atomic_store_n` | `GdwuPYbVpP4` |
| `__fflush` | `E1iwBYkG3CM` |
| `__kernel_cos` | `-m7FIvSBbMM` |
| `__kernel_cosdf` | `7ruwcMCJVGI` |
| `__kernel_sindf` | `2Lvc7KWtErs` |
| `__longjmp` | `9mKjVppFsL0` |
| `__vfprintf` | `yAZ5vOpmBus` |
| `_init_tls` | `6NCOqr3cD74` |
| `_malloc_finalize_lv2` | `RnqlvEmvkdw` |
| `_malloc_fini` | `21KFhEQDJ3s` |
| `_malloc_init_lv2` | `20cUk0qX9zo` |
| `_malloc_init` | `z8GPiQwaAEY` |
| `_malloc_postfork` | `V94pLruduLg` |
| `_malloc_prefork` | `aLYyS4Kx6rQ` |
| `_new_setup` | `KNNNbyRieqQ` |
| `_readdir_unlocked` | `-u3XfqNumMU` |
| `_rtld_atfork_pre` | `i-7v8+UGglc` |
| `_rtld_error` | `dmHEVUqDYGw` |
| `_rtld_get_stack_prot` | `AdYYKgtPlqg` |
| `_rtld_thread_init` | `gRw4XrztJ4Q` |
| `_sceLibcGetMallocParam` | `1nZ4Xfnyp38` |
| `_seekdir` | `Do3zPpsXj1o` |
| `backtrace_game` | `jFSmlgyyEUg` |
| `clock_1700` | `n8onIBR4Qdk` |
| `devname_r` | `BIALMFTZ75I` |
| `devname` | `fqLrWSWcGHw` |
| `malloc_initialize` | `owT6zLJxrTs` |
| `Unknown` | `GG6441JdYkA` |
| `Unknown` | `QZ9YgTk+yrE` |
| `Unknown` | `bGuDd3kWVKQ` |
| `Unknown` | `f9LVyN8Ky8g` |
| `Unknown` | `o1Ez0fsXvhI` |
| `Unknown` | `wUqJ0psUjDo` |

### Unknown function info

- [Additional libSceLibcInternal exports](../../../core/libs/prx/libSceLibcInternal/Export.cpp) - the name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscelibcinternal-f9a6d6abff.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `sceLibcMspaceSetMallocCallback` | `+CbwGRMnlfU` |
| `sceLibcPafMspaceReportMemoryBlocks` | `0FnzR6qum90` |
| `sceLibcHeapSetAddressRangeCallback` | `4iOzclpv1M0` |
| `sceLibcPafMspaceTrim` | `AUYdq63RG3U` |
| `sceLibcHeapMutexCalloc` | `HFtbbWvBO+U` |
| `sceLibcInternalSetMallocCallback` | `LXqt47GvaRA` |
| `sceLibcHeapSetTraceMarker` | `M6qiY0nhk54` |
| `sceLibcPafMspaceCheckMemoryBounds` | `Pcq7UoYAcFE` |
| `sceLibcHeapUnsetTraceMarker` | `RlhJVAYLSqU` |
| `sceLibcHeapGetAddressRanges` | `Sj3fKG7MwMk` |
| `sceLibcHeapMutexFree` | `jJKMkpqQr7I` |

- [_sceLibcInternalThreadAtexit, _sceLibcInternalThreadDtors](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - FreeBSD `__cxa_thread_atexit` semantics assumed; a null or non-image destructor and an allocation failure throw
- [_sceLibcInternalForceTlsDestructor](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - behaviour follows `sceLibcForceTlsDestructor(int)` in a title's `libc.prx` (disassembly): the calling thread's destructors whose dso belongs to the module run newest first, only if the dso's module is loaded and the dso points to itself, in up to 4 passes. The return type is unknown (that function returns nothing), 0 is returned. The module of each dso is looked up when the destructors are forced, not when they are registered as `libc.prx` does

### Functional

- [makecontext](../../../core/libs/prx/libSceLibcInternal/UserContext.cpp) (libSceLibcInternal) follows FreeBSD 11's `lib/libc/amd64/gen/makecontext.c`: at most six arguments, a stack of at least `MINSIGSTKSZ` (2048) bytes, and `exit(0)` when the function returns with no `uc_link`. The console's version is not verified
- [makecontext](../../../core/libs/prx/libSceLibcInternal/UserContext.cpp) (libSceLibcInternal) on Windows pins its stack writable until a later `makecontext` stack overlaps it, since nothing tells the library when the guest frees the stack
- [Thread exit destructors](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) run when a libkernel thread finishes; on the main or a host thread only `_sceLibcInternalThreadDtors` runs them, as `exit` does, else they are dropped
- [__opendir2](../../../core/libs/prx/libSceLibcInternal/Directory.cpp) (libSceLibcInternal, NID `KOy7MeQ7OAU`) - the device's answer to any flag is not measured; the flags follow FreeBSD 11.4's [`__opendir2`](https://github.com/freebsd/freebsd-src/blob/d106002967eff712894ab8b5337bbbff106ce5e0/lib/libc/gen/opendir.c#L76-L95)
- [fdopendir](../../../core/libs/prx/libSceLibcInternal/Directory.cpp) (libSceLibcInternal) follows FreeBSD 11's [`fdopendir`](https://github.com/freebsd/freebsd-src/blob/release/11.0.0/lib/libc/gen/opendir.c#L67-L74): it sets `FD_CLOEXEC` on the descriptor first, and the stream starts at the descriptor's current position instead of the first entry. On Windows the stream skips the entries the descriptor's cursor (`lseek` or `getdirentries`) has passed, which assumes that the descriptor and the stream list a directory in the same host order; on Linux the stream is opened on a duplicate of the descriptor, which shares its position. The Linux path is not run by the tests here. As in FreeBSD, a descriptor that is not open fails with `EBADF`, one that is not a directory with `ENOTDIR` (both leave it open, and `FD_CLOEXEC` stays set on the latter), and `closedir` closes it
- [fstatvfs](../../../core/libs/prx/libSceLibcInternal/FilesystemStatistics.cpp) (libSceLibcInternal) converts `_fstatfs` to the 88-byte `struct statvfs` as FreeBSD 9.2's `sfs2svfs` does (`f_bsize` from the transfer size, `f_frsize` from the block size, `f_favail` equal to `f_ffree`, `f_fsid` 0, `ST_RDONLY` and `ST_NOSUID` only). Differences: `f_namemax` is the `statfs` value, not `fpathconf(_PC_NAME_MAX)`; negative `f_bavail` and `f_ffree` wrap to unsigned without `EOVERFLOW`, as the same-width copy in FreeBSD does; inode counts are 0 on Windows, inherited from `_fstatfs`; a null output returns `EFAULT` after the descriptor is validated, where FreeBSD would fault. Layout and conversion are unverified against the PS5 library (PS5MC imports `fstatvfs` but was not run)
- [posix_spawn and posix_spawnp](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal) create a native child running the requested program's converted image, which must be under `/app0/` (`/app0/eboot.bin` maps to the running image); `posix_spawnp` searches `PATH` as FreeBSD 11's `_execvpe`, and the FreeBSD 11.4 file actions and attributes are applied to the child. A null `argv` returns `EFAULT`; a null `envp` inherits the parent environment; guest process IDs are the host's. Limits: the child executes the conversion made at relink time; Windows redirects only descriptors 0 to 2 and starts the child on the parent's std handles; Linux applies only `POSIX_SPAWN_RESETIDS` and `POSIX_SPAWN_SETPGROUP`, other flags or a nonzero set on Windows throw. The console sandbox's effect on child creation is unverified, and null or uninitialized objects, which FreeBSD dereferences, throw. PPSA99305 (Dusklight) reaches this when SDL looks for `zenity`
- [getopt, getopt_long](../../../core/libs/prx/libSceLibcInternal/Getopt.cpp) (libSceLibcInternal) follow FreeBSD 11 [getopt.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/getopt.c) and [getopt_long.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/getopt_long.c) with `GNU_COMPATIBLE`: `getopt` does not permute, `getopt_long` permutes unless `POSIXLY_CORRECT` is set in the guest environment or the option string starts with `+`, and the exported `opterr`, `optind`, `optopt`, `optreset` and `optarg` start at 1, 1, 0, 0 and null. Warnings go to the guest's current `stderr` prefixed with `__progname`, where FreeBSD's `warnx` keeps the `stderr` of its first call. A null `getopt` option string, a null argument vector or argument, a negative `optind` and an `optind` moved inside a pending permutation throw where FreeBSD faults. `getopt_long_only` is not exported. That the console's `libSceLibcInternal` uses this FreeBSD code is unverified
- [posix_spawnattr_getflags, getpgroup, getschedparam, getschedpolicy, getsigdefault, getsigmask, setpgroup, setschedparam, setschedpolicy and setsigmask](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal) read and write the 0x30-byte attribute object at the offsets of the console's `libSceLibcInternal.sprx` (SDK 4.5x build of `J03950001`, 0x8D860 to 0x8D910: flags at 0, process group at 4, scheduling parameter at 8, policy at 0xC, default and masked signal sets at 0x10 and 0x20), which never fail there and return 0; the layout matched on a PS5 (firmware 13.60). Uninitialized attributes and null pointers, which the console dereferences, throw as the existing setters do
- [sys_siglist and sys_nsig](../../../core/libs/prx/libSceLibcInternal/SignalList.cpp) (libSceLibcInternal) hold the 32 descriptions and the count of [FreeBSD 11.4](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/siglist.c#L73-L107) `siglist.c` (`NSIG` 32, signal 0 to `SIGUSR2`); the console's table was not read
- [_err, _warn](../../../core/libs/prx/libSceLibcInternal/Err.cpp) and [_getprogname](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) are assumed to be the same functions as `err`, `warn` and `getprogname` and run their code; the console's module was not read. FreeBSD 11.4 defines the underscore names and makes the public names weak references to them ([`_err`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/err.c#L70-L79), [`_warn`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/err.c#L135-L144), [`_getprogname`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/getprogname.c#L10-L17))
- [inet_aton, inet_addr, inet_ntoa and inet_ntoa_r](../../../core/libs/prx/libSceLibcInternal/Inet.cpp) (libSceLibcInternal) follow the FreeBSD code in the console's `libSceLibcInternal.sprx` (SDK 4.5x build of `J03950001`, 0x8ADE0 to 0x8B3D0) and matched it on a PS5 (firmware 13.60) for 20 `inet_aton` inputs; a null text, which the console dereferences, throws. `inet_ntoa_r` formats through libkernel's `__inet_ntop`, so a null buffer with room for the text throws and a null buffer that is too small sets `ENOSPC`, as `__inet_ntop` does for a null destination; the console's `inet_ntoa_r` was not measured with a null buffer
- [__inet_ntoa_r](../../../core/libs/prx/libSceLibcInternal/Inet.cpp) (libSceLibcInternal) was not measured on the console: it runs `inet_ntoa_r`, as FreeBSD 11.4 names the function `__inet_ntoa_r` ([arpa/inet.h](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/arpa/inet.h#L129)) and makes `inet_ntoa_r` a weak alias of it ([inet_ntoa.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet/inet_ntoa.c#L62-L76))

### Additional PS5 3.20 imports (xdCloudy #9498)

- [3.20 pending imports](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - The following PS5 3.20 inventory names are declared for import resolution only; their prototypes and behavior are unverified, and each call throws through `NotImplemented_nid_no_patch`. The source list is [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs).
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Atan`, `_Atqexit`, `_Atthreadexit`, `_CStrftime`, `_CStrxfrm`, `_CWcsxfrm`, `_Call_onceEx`, `_Closreg`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Daysto`, `_Deletegloballocale`, `_Do_call`, `_Dtento`, `_Erfc`, `_FAtan`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_FDtento`, `_FErfc`, `_FHypot`, `_FLogpoly`, `_FPlsw`, `_FPmsw`, `_FPow`, `_FQuad`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_FQuadph`, `_FRint`, `_FSincos`, `_FTan`, `_FTgamma`, `_FXp_addh`, `_FXp_addx`, `_FXp_getw`, `_FXp_invx`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_FXp_ldexpx`, `_FXp_movx`, `_FXp_mulh`, `_FXp_mulx`, `_FXp_setn`, `_FXp_setw`, `_FXp_sqrtx`, `_FXp_subx`, `_Fac_tidy`, `_Fail_s`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - 
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Fgpos`, `_Fofind`, `_Fofree`, `_Fopen`, `_Foprep`, `_Frprep`, `_Fspos`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Fwprep`, `_Genld`, `_Gentime`, `_Getcloc`, `_Getctyptab`, `_Getdst`, `_Geterrno`, `_Getfld`, `_Getfloat`, `_Getgloballocale`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Getint`, `_Getpcostate`, `_Getpwcostate`, `_Getpwctrtab`, `_Getpwctytab`, `_Getstr`, `_Gettime`, `_Getzone`, `_Hypot`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Isdst`, `_LAtan`, `_LDtento`, `_LErfc`, `_LHypot`, `_LLogpoly`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_LPlsw`, `_LPmsw`, `_LPow`, `_LQuad`, `_LQuadph`, `_LRint`, `_LSincos`, `_LTan`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_LTgamma`, `_LXp_addh`, `_LXp_addx`, `_LXp_getw`, `_LXp_invx`, `_LXp_ldexpx`, `_LXp_movx`, `_LXp_mulh`, `_LXp_mulx`, `_LXp_setn`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_LXp_setw`, `_LXp_sqrtx`, `_LXp_subx`, `_Ldtob`, `_Litob`, `_Lock_spin_lock`, `_Locsum`, `_Locterm`, `_Locvar`, `_Logpoly`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Makeloc`, `_Makestab`, `_Makewct`, `_Mbtowc`, `_Mtxdst`, `_Mtxinit`, `_Mtxlock`, `_Mtxunlock`, `_Nnl`
- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Plsw`, `_Pmsw`, `_Pow`, `_Printf`, `_Putfld`, `_Putstr`, `_Puttxt`, `_Quad`, `_Quadph`, `_Readloc`

### Additional PS5 3.20 imports (round 6)

- [_ZSt8_XLgammaf](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `DWMcG8yogkY`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIcE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `CKlZ-H-D1CE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZnamSt11align_val_tRKSt9nothrow_t](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `s2eGsgUF9vk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `V2FICbvPa+s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_thread_init](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `PJW+-O4pj6I`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt15_sceLibcLocinfoPKc](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zugltxeIXM0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `hnGhTkIDFHg`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `XcuCO1YXaRs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6iDi6e2e4x8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_istreamIwSt11char_traitsIwEED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `StJaKYTRdUE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIwEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `RrTMGyPhYU4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIcSt11char_traitsIcEE9underflowEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Nl6si1CfINw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIcSt11char_traitsIcEE9_EndwriteEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `JTwt9OTgk1k`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6locale6globalERKS_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `ptwhA0BQVeE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `gOzIhGUAkME`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIcEC2Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6DBUo0dty1k`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIcEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Oo1r8jKGZQ4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [signalcontext](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `SQGxZCv3aYk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `dOKh96qQFd0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `u5yK3bGG1+w`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZTv0_n24_NSiD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `0bGGr4zLE3w`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7codecvtIwc9_MbstatetED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `9BI3oYkCTCU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7codecvtIcc9_MbstatetEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `XhwSbwsBdx0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8numpunctIcEC1EPKcmb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `WK24j1F3rCU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIcSt11char_traitsIcEE9pbackfailEi](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `jerxcj2Xnbg`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `U4JP-R+-70c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt22_Future_error_categoryD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `xsXQD5ybREw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [__srefill](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `fIskTFX9p68`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIwE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Zz-RfDtowlo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `m2lChTx-9tM`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZdaPvmRKSt9nothrow_t](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `7lCihI18N9I`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [sceLibcPafMspaceMallocUsableSize](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6JcY5RDA4jY`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `RV6sGVpYa-o`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `vu0B+BGlol4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt5ctypeIwED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `P0383AW3Y9A`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZdlPvmRKSt9nothrow_t](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `7VPIYFpwU2A`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_get_malloc_state](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Nmezc1Lh7TQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `X3DrtC2AZCI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt18bad_variant_accessD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `U3b5A2LEiTc`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `9pPbNXw5N9E`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7codecvtIcc9_MbstatetED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `to7GggwECZU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt8_XLgammae](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `bl0DPP6kFBk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt15underflow_errorD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `iHILAmwYRGY`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [sceLibcMspaceGetAddressRanges](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `raRgiuQfvWk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt14overflow_errorD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `y1dYQsc67ys`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6locale7_LocimpC1Eb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6b3KIjPD33k`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_report_memory_blocks](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `0F08WOP8G3s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8_LocinfoD2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `YXVCU6PdgZk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8numpunctIwE5_InitERKSt8_Locinfob](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `uiRALKOdAZk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_reclaim_telldir](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `7ZFy2m9rc5A`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE9_EndwriteEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `cV6KpJiF0Ck`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZdaPvSt11align_val_t](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `v09ZcAhZzSc`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `p1WMhxL4Wds`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_stats](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `CC-BLMBu9-I`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIcEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `SD403oMc1pQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt10moneypunctIwLb1EED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `fWuQSVGOivA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE9underflowEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `hXsvfky362s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIcEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `splBMMcF3yk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE7seekoffElNSt5_IosbIiE8_SeekdirENS4_9_OpenmodeE](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `g7gjCDEedJA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `iTODM3uXS2s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `8PJ9qmklj2c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt17iostream_categoryv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `V23qt24VPVs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7_MpunctIcEC2Emb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zWSNYg14Uag`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt16nested_exceptionD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `za50kXyi3SA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_thread_init_stub](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `1kIpmZX1jw8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt24_Iostream_error_categoryD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `jVwxMhFRw8Q`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7_MpunctIwEC2Emb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Pr1yLzUe230`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt23_Generic_error_categoryD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `YBrp9BlADaA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6locale7_LocimpC2Eb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zrmR88ClfOs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `1gYJIrfHxkQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `CiD6-BPDZrA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `x5yAFCJRz8I`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIcSt11char_traitsIcEE5imbueERKSt6locale](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `qM0gUepGWT0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt10_GetloctxtIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEiRT0_S5_mPKT_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `bsohl1ZrRXE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [sceLibcPafMspaceGetFooterValue](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `kv4kgdjswN0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [sceLibcInternalMemoryGetWakeAddr](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `YrL-1y6vfyo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7_MpunctIcED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Lzj4ws7DlhQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `L7Vnk06zC2c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [__atomic_load_n](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Q6oqEnefZQ8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8ios_base4InitC2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `bTQcNwRc8hE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt4_PadD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `XyJPhPqpzMw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_malloc_thread_cleanup](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Sopthb9ztZI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `h+c9OSfCAEg`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8ios_baseD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `X9D8WWSG3As`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt9_LStrcollIwEiPKT_S2_S2_S2_PKSt8_Collvec](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `m6uU37-b27s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `bY9Y0J3GGbA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIwED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `gMwkpZNI9Us`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9exception18_Set_raise_handlerEPFvRKS_E](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Bin7e2UR+a0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6_MutexD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `djHbPE+TFIo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7codecvtIDsc9_MbstatetED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Oeo9tUbzW7s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIwEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `4Srtnk+NpC4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIwEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `d-MOtyu8GAk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_rtld_atfork_post](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `R2QKo3hBLkw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_finalize](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `J6FoFNydpFI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt10unexpectedv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `lKfKm6INOpQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt10moneypunctIwLb0EEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `jna5sqISK4s`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6locale7_LocimpD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `bleKr8lOLr8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_rtld_addr_phdr](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `5FoE+V3Aj0c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_check_memory_bounds](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `ECOPpUQEch0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIcEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `969Euioo12Q`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIcEC1ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `raLgIUi3xmk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_St22_String_const_iteratorISt11_String_valISt13_Simple_typesIwEEEm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `-hrHhi-UFxs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIcEC1EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `W0w8TGzAu0Q`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `ZolDcuDSD0Q`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIcED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `jy9urODH0Wo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt10moneypunctIwLb1EEC2ERKSt8_Locinfomb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `H0a2QXvgHOk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE5uflowEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `VrXGNMTgNdE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZTv0_n24_NSt13basic_istreamIwSt11char_traitsIwEED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `izmoTISVoF8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [__kernel_sin](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zpy7LnTL5p0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `IuFImJ5+kTk`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt8_XLgammad](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `yYk819F9TyU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6yplvTHbxpE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [malloc_get_footer_value](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `SlG1FN-y0N0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `DTH1zTBrOO8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt22_Get_future_error_whati](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `cPNeOAYgB0A`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [sceLibcInternalMemoryMutexEnable](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `h8jq9ee4h5c`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `13xzrgS8N4o`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [__kernel_rem_pio2](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `GLNDoAYNlLQ`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8numpunctIcED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `xFva4yxsVW8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7collateIcEC2EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `VClCrMDyydE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `+WvmZi3216M`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt18bad_variant_accessD0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `8DNJW5tX-A8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `ryl2DYMxlZ0`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7_MpunctIwED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `8UeuxGKjQr8`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6_MutexC1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `2KNnG2Z9zJA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEE5imbueERKSt6locale](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `kmzNbhlkddA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `qkl3Siab04M`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8numpunctIcED0Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Lf6h5krl2fA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Zqc++JB04Qs`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8_LocinfoC1EiPKc](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `L1Ze94yof2I`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `6NH0xVj6p7M`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt10moneypunctIwLb0EEC1Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `McUBYCqjLMg`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `yofHspnD9us`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `0Pd-K5jGcgM`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8messagesIwEC2Em](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `x8PFBjJhH7E`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6_WinitD2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `2yOarodWACE`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt6_WinitD1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Uw3OTZFPNt4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt10moneypunctIwLb0EEC1ERKSt8_Locinfomb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `dHs7ndrQBiI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7codecvtIcc9_MbstatetE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `iZCHNahj++4`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED1Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zkCx9c2QKBc`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `X1DNtCe22Ks`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `aTjYlKCxPGo`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `XBmd6G-HoYI`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt8numpunctIcEC2ERKSt8_Locinfomb](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `Cb1hI+w9nyU`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt3pmr20null_memory_resourceEv](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `eWMGI7B7Lyc`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5_](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `zTX7LL+w12Q`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt13basic_filebufIwSt11char_traitsIwEED2Ev](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `aC9OWBGjvxA`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt3pmr20set_default_resourceEPNS_15memory_resourceE](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `TKYsv0jdvRw`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
- [_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo](../../../core/libs/prx/libSceLibcInternal/InventoryExports6.cpp) (libSceLibcInternal, NID `cXL+LN0lSwc`) - listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceLibcInternal.c); signature and behavior are unverified, so this parameterless declaration is for import resolution only and throws through `NotImplemented_nid_no_patch`
