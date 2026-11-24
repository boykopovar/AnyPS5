# libSceNKWebKit

## add Functional

- [WTF::URL setters](../../../core/libs/prx/libSceNKWebKit/src/WtfUrlParser.cpp) (libSceNKWebKit) - not compared with a console module. `setProtocol`, `setHost`, `setHostAndPort`, `setUser`, `removeCredentials`, `setFragmentIdentifier`, `setQuery`, `setPath`, `truncatedForUseAsBase` and `fileURLWithFileSystemPath` follow the [URL.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531a/Source/WTF/wtf/URL.cpp) of that revision and re-parse through the URL parser, checked against a Linux build of it. As there, `setProtocol` returns whether the scheme was accepted, and percent-encoding a user or path copies its UTF-8 bytes in as Latin-1 characters, so non-ASCII characters are encoded twice. `fakeURLWithRelativePart` still throws: it needs `libSceJsc`'s `createCanonicalUUIDString`. A null object and a view with length but no characters throw.

## remove | `_ZN3WTF3URL11setProtocolENS_10StringViewE` | `dnZM2KrZii0` |

## remove | `_ZN3WTF3URL7setHostENS_10StringViewE` | `RoG6zFjMczI` |

## remove | `_ZN3WTF3URL14setHostAndPortENS_10StringViewE` | `w1JXzWCP5UM` |

## remove | `_ZN3WTF3URL7setUserENS_10StringViewE` | `K79kvESFqPQ` |

## remove | `_ZN3WTF3URL17removeCredentialsEv` | `U0i8CT7bRsg` |

## remove | `_ZN3WTF3URL21setFragmentIdentifierENS_10StringViewE` | `piUmuhIuQBs` |

## remove | `_ZN3WTF3URL8setQueryENS_10StringViewE` | `EFDd0QoYyP4` |

## remove | `_ZN3WTF3URL7setPathENS_10StringViewE` | `LZyPi2q90gA` |

## remove | `_ZNK3WTF3URL21truncatedForUseAsBaseEv` | `uekVJTVqBGc` |

## remove | `_ZN3WTF3URL25fileURLWithFileSystemPathENS_10StringViewE` | `kQyNibZId5k` |
