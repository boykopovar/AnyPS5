# libSceNKWebKit

## add Functional

- [WKGeolocationManager provider, updates and context access](../../../core/libs/prx/libSceNKWebKit/src/GeolocationManager.cpp) (libSceNKWebKit) - `WKContextGetGeolocationManager`, `WKGeolocationManagerSetProvider`, `WKGeolocationManagerProviderDidChangePosition`, `WKGeolocationManagerProviderDidFailToDeterminePosition` and `WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage` create a host `WKGeolocationManager` per context and store provider and position/error state in host memory; provider client callbacks are not invoked, and WebKit geolocation service integration is unmodeled.

## remove | `WKContextGetGeolocationManager` | `GJsL9usTdig` |

## remove | `WKGeolocationManagerProviderDidChangePosition` | `NB5lJxGW9Ro` |

## remove | `WKGeolocationManagerProviderDidFailToDeterminePosition` | `0jIcJu0tb+o` |

## remove | `WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage` | `NTLwhm+Wfk8` |

## remove | `WKGeolocationManagerSetProvider` | `55QR+rnq6FI` |
