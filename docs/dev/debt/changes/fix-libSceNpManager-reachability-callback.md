# libSceNpManager

## add Functional

- [sceNpRegisterNpReachabilityStateCallback](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - keeps one callback, `(int userId, uint32_t state, void* userdata)`, and `sceNpCheckCallback` and `sceNpCheckCallbackForLib` call it once after each registration with the initial user and `UNAVAILABLE` (0), the state `sceNpGetNpReachabilityState` reports; it is never called again, as that state never changes, and unregistering before the check cancels the delivery. The callback type follows shadPS4's reachability callback and the one-delivery-per-registration model the state callbacks above use; whether the console delivers an initial state at registration, for which user and at which poll is not measured. A second registration returns `CALLBACK_ALREADY_REGISTERED`, as for the state callbacks

## change [NP reachability callbacks]

- [NP reachability callbacks](../../../core/libs/prx/libSceNpManager/Export.cpp) retain one callback and its user data; duplicate registration and unregistering an absent callback return the errors used by [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/945dbc3cc3eee80ac3e053b438502ed936fa6bb2/src/core/libraries/np/np_manager.cpp). The one delivery of the unavailable state is described in the `sceNpRegisterNpReachabilityStateCallback` entry.
