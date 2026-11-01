# libSceNpPartnerSubscription technical debt

### Unknown function info

- [libSceNpPartnerSubscription named exports](../../../core/libs/prx/libSceNpPartnerSubscription/Export.cpp) - signatures and behavior are unknown. The parameterless `int` declarations are import placeholders, not ABI prototypes, and immediately throw through `NotImplemented_nid_no_patch`. The pinned [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscenppartnersubscription-5a04610102.jsonl) identifies the two name/NID pairs below; the pinned [module map](https://github.com/boykopovar/ps5-nid-map/blob/cc1d586fddab576a520f226da6b2632b031e6d09/data/Modules/libSceNpPartnerSubscription.inc) assigns them to `libSceNpPartnerSubscription.sprx`.
- [Unresolved exports](../../../core/libs/prx/libSceNpPartnerSubscription/Export.cpp) - PartnerSubscription NIDs `8w1khP7hjwE` and `J8xmLvppAtA` have unresolved names. The same physical module also provides the separate logical library `libSceNpUbisoftPlus`; its [four inventory NIDs](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscenpubisoftplus-6af4f14135.jsonl) are ownership-pending and have unresolved names. None of those six is declared.

| Name | NID |
| --- | --- |
| `sceNpPartnerSubscriptionInitialize` | `gLYGYNEMo3o` |
| `sceNpPartnerSubscriptionTerminate` | `Xd00XKD1NSM` |
