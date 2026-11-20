# libSceIcu

## change [ICU IDNA exports]

- [ICU IDNA exports](../../../core/libs/prx/libSceIcu/Idna.cpp) are unverified against console ICU 68/Unicode 13. The four existing C exports use the pinned ICU 78.1 UTS46 implementation and its source-built Unicode 17 normalization data, so mappings, validity and error flags may differ for later characters. Forged, closed and inaccessible handles or pointers throw rather than accessing host memory; console behavior for invalid arguments beyond the native ICU contract is unverified. Each open keeps a distinct guest identity until process teardown and releases its native object on close. ICU allocator reconfiguration and cleanup still require no outstanding handles or concurrent use. Closes when: compared with the console, including Unicode-version-sensitive domains and the guest ABI.
