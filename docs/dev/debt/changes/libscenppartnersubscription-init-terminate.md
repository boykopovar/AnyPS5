# libSceNpPartnerSubscription

## add Functional

- [sceNpPartnerSubscriptionInitialize, sceNpPartnerSubscriptionTerminate](../../../core/libs/prx/libSceNpPartnerSubscription/Export.cpp) (libSceNpPartnerSubscription) - initialization marks the library initialized and termination clears it, modelled on the sibling libSceNpPartner001 pair; no console measurement or titles' use is known. A second initialization and termination without initialization throw because the console's error codes are unknown.
