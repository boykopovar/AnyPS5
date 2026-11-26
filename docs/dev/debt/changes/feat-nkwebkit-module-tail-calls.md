# libSceNKWebKit

## add Functional

- [More forwarded exports](../../../core/libs/prx/libSceNKWebKit/src/ForwardExports.cpp) (libSceNKWebKit) - 17 more exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is a `jmp` that reaches another export, directly or through one or two further unexported `jmp`s, implemented as an assembly tail jump to that export like the existing forwarded exports: 2 `FrameLoaderClient` `operator new`/`new[]` to `WTF::fastMalloc`, 3 `operator delete`/`delete[]` and the `LibWebRTCProvider` deleting destructor to `WTF::fastFree`, 10 generated `JS*::finishCreation` to their base class's `finishCreation`, and the two `JSDOMWindow` destructors to `JSDOMWindowBase::~JSDOMWindowBase`, which still throws.
- [More empty-body exports](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - 10 exports whose body in the same module is a `jmp` (through up to two more) that ends at a bare `ret`, including `JSNode::finishCreation`, the base that the forwarded `finishCreation` chains end at.

## remove | `WKBundleRemoveAllWebNotificationPermissions` | `ldxacgzSYVM` |

## remove | `WKBundleSetServiceWorkerProxyCreationCallback` | `tow2sT8FF3c` |

## remove | `WKContextTerminateServiceWorkers` | `i8HHFDiCd4U` |

## remove | `WKSendDisplayConfigurationChangedMessageForTesting` | `CQsllLq8d7I` |

## remove | `WKTextCheckerSetTestingMode` | `uhbwdrVWKnk` |
