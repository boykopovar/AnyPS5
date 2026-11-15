# libSceNKWebKit

## add Functional

- [WebCore::LoggedInStatus](../../../core/libs/prx/libSceNKWebKit/src/LoggedInStatus.cpp) (libSceNKWebKit) - not compared with a console module. The 40-byte object and the 24-byte `ExceptionOr<UniqueRef<LoggedInStatus>>` (object pointer or `Exception`, has-value byte at 0x10) follow the pinned [LoggedInStatus.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/LoggedInStatus.h) and `ExceptionOr.h`, measured on a Linux build of those headers. The object is allocated with `fastMalloc`, as the PS5 port builds with `USE_ISO_MALLOC` off. Whitespace above Latin-1 uses `libSceIcu`'s `u_charDirection`, whose Unicode data may differ from the console's. Null references and a null object throw; the source only asserts in `hasExpired`.

## remove | `_ZN7WebCore14LoggedInStatus13setTimeToLiveEN3WTF7SecondsE` | `3DFYSB4Xk68` |

## remove | `_ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeE` | `NDfv8voZjPU` |

## remove | `_ZN7WebCore14LoggedInStatus6createERKNS_17RegistrableDomainERKN3WTF6StringENS0_19CredentialTokenTypeENS0_18AuthenticationTypeENS4_7SecondsE` | `iZ+sJFzLU7U` |

## remove | `_ZNK7WebCore14LoggedInStatus10hasExpiredEv` | `mgNAFnYj41Q` |

## remove | `_ZNK7WebCore14LoggedInStatus6expiryEv` | `0uCtvJ7t7X8` |
