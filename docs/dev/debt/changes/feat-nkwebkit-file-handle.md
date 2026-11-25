# libSceNKWebKit

## add Functional

- [WebCore::FileHandle](../../../core/libs/prx/libSceNKWebKit/src/FileHandle.cpp) (libSceNKWebKit) - not compared with a console module. The 24-byte layout and the members' defaults follow the pinned [FileHandle.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/FileHandle.h), measured on a Linux build of it; `read` and `write` retry on guest `EINTR` as the pinned POSIX `readFromFile` and `writeToFile` do. As in the [source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/FileHandle.cpp), the move constructor does not carry the lock settings, and a locking `close` keeps the descriptor, so a later `close` or the destructor unlocks and closes it again. `printf` formats through the guest `vsnprintf`. A null object or path throws.

## remove | `_ZN7WebCore10FileHandle4openERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE` | `hFk9FKLENG8` |

## remove | `_ZN7WebCore10FileHandle4openEv` | `FUbGs1rzI6g` |

## remove | `_ZN7WebCore10FileHandle4readEPvi` | `tmUOdbwJw1E` |

## remove | `_ZN7WebCore10FileHandle5closeEv` | `MbuvuN84TNc` |

## remove | `_ZN7WebCore10FileHandle5writeEPKvi` | `UpER4IpzKH0` |

## remove | `_ZN7WebCore10FileHandle6printfEPKcz` | `WEVIlPFp9Rk` |

## remove | `_ZN7WebCore10FileHandleC1EOS0_` | `KPJLpACpg6k` |

## remove | `_ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE` | `e-mG5kgUUIM` |

## remove | `_ZN7WebCore10FileHandleC1ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE` | `oT0mxPLVvvQ` |

## remove | `_ZN7WebCore10FileHandleC1Ev` | `l9nuY5Wn5+c` |

## remove | `_ZN7WebCore10FileHandleC2EOS0_` | `c2EZZ1IdQAE` |

## remove | `_ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeENS1_9OptionSetINS5_12FileLockModeEEE` | `sfw1zSO4sd8` |

## remove | `_ZN7WebCore10FileHandleC2ERKN3WTF6StringENS1_14FileSystemImpl12FileOpenModeE` | `c9v8NHVu0NU` |

## remove | `_ZN7WebCore10FileHandleC2Ev` | `ctto9lYZYhA` |

## remove | `_ZN7WebCore10FileHandleD1Ev` | `dcRpnwhqCwc` |

## remove | `_ZN7WebCore10FileHandleD2Ev` | `0tjbqZDVPp8` |

## remove | `_ZN7WebCore10FileHandleaSEOS0_` | `Em5voBytaq0` |

## remove | `_ZNK7WebCore10FileHandlecvbEv` | `1WM6fw+AXm0` |
