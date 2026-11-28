# libSceNKWebKit

## add Functional

- [WebCore::SQLiteDatabase and SQLiteTransaction](../../../core/libs/prx/libSceNKWebKit/src/SQLite.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The exports carry 3.20 signatures (`open(const String&, OpenMode)`, `executeCommand(const String&)`, `tableExists(const String&)`) that the pinned 11.x tree no longer has, so they follow August 2020 [SQLiteDatabase.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteDatabase.cpp#L48-L631) and [SQLiteTransaction.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteTransaction.cpp#L37-L104). They call the libSceNKWebKitRequirements SQLite exports. Closes when: compared with a console module.
- [WebCore::SQLiteDatabase::tableExists](../../../core/libs/prx/libSceNKWebKit/src/SQLite.cpp) (libSceNKWebKit) - not compared with a console module. It binds the name, as the pinned 11.x [tableSQL](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteDatabase.cpp#L458-L472) does; August 2020 [tableExists](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteDatabase.cpp#L384-L394) concatenates it, so the two differ for a name holding a single quote. It binds and reads UTF-16 through the 3.20 statement, where 11.x [SQLiteStatement.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteStatement.cpp#L119-L235) uses UTF-8. Closes when: compared with a console module.
- [WebCore::SQLiteStatement and SQLiteStatementAutoResetScope](../../../core/libs/prx/libSceNKWebKit/src/SQLite.cpp) (libSceNKWebKit) - not compared with a console module. The 3.20 `SQLiteStatement(SQLiteDatabase&, const String&)`, `prepare`, `finalize` and `getColumn*` exports, which the pinned 11.x tree replaced with `SQLiteDatabase::prepareStatement`, follow August 2020 [SQLiteStatement.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteStatement.cpp#L44-L468) and [SQLiteStatementAutoResetScope.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteStatementAutoResetScope.cpp#L34-L55). Closes when: compared with a console module.
- [WebCore SQLite object layouts](../../../core/libs/prx/libSceNKWebKit/src/SQLiteAbi.hpp) (libSceNKWebKit) - release layouts of August 2020 [SQLiteDatabase.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteDatabase.h#L48-L181) (64 bytes, `WTF::Lock` bytes at 14, 24 and 40, `m_lastChangesCount` at 56) and [SQLiteStatement.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/sql/SQLiteStatement.h#L35-L102) (24 bytes); `SQLiteTransaction` is 16 bytes. The pinned [11.x layout](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/sql/SQLiteDatabase.h#L52-L205) differs: it derives from `CanMakeWeakPtr` and drops `m_lastChangesCount`. None of the 55 SQLite names that only the pin declares, such as its `open` (`lCTp1S9tQ5Q`) and `prepareStatement`, is in the inventory. Closes when: measured on a PS5.
- [WebCore SQLite wrappers](../../../core/libs/prx/libSceNKWebKit/src/SQLite.cpp) (libSceNKWebKit) - these inputs throw through `NotImplemented_nid_no_patch`: `isColumnDeclaredAsBlob` for a column without a declared type, where WebKit passes the null `sqlite3_column_decltype` result to `strlen`, or for SQL that `prepare` leaves without a statement; `runIncrementalVacuumCommand` and `setCollationFunction` on a closed database, which SQLite built without API armor dereferences and the console SQLite build is unknown; `bindValue` on a valueless `WTF::Variant`; a collation call through an empty `WTF::Function`. Closes when: measured on a PS5.
- [WebCore SQLite wrappers](../../../core/libs/prx/libSceNKWebKit/src/SQLite.cpp) (libSceNKWebKit) - `WTF::Lock` contention yields until the byte is free instead of parking on `WTF::ParkingLot`, whose exports still throw. `open` leaves `m_openingThread` null because `WTF::Thread::current` is not implemented; release builds read it only in assertions. `m_authorizer` stays null because `setAuthorizer` is not exported. SQLite results are those of the bundled SQLite 3.54.1. Closes when: WTF::ParkingLot and WTF::Thread are implemented.

## remove | `_ZN7WebCore14SQLiteDatabaseC1Ev` | `dYLaCFvC8Nk` |

## remove | `_ZN7WebCore14SQLiteDatabaseC2Ev` | `B6gqPm8EtR8` |

## remove | `_ZN7WebCore14SQLiteDatabaseD1Ev` | `z-Vtoq+OkUU` |

## remove | `_ZN7WebCore14SQLiteDatabaseD2Ev` | `ox8PemJycj8` |

## remove | `_ZN7WebCore14SQLiteDatabase5closeEv` | `cL5ASE2-dwg` |

## remove | `_ZN7WebCore14SQLiteDatabase14clearAllTablesEv` | `Jr8SilBIzTk` |

## remove | `_ZN7WebCore14SQLiteDatabase16runVacuumCommandEv` | `QJCcZ0vgv54` |

## remove | `_ZN7WebCore14SQLiteDatabase27runIncrementalVacuumCommandEv` | `jmHFoFUQoi8` |

## remove | `_ZN7WebCore14SQLiteDatabase9interruptEv` | `8m8-UQTKyUc` |

## remove | `_ZN7WebCore14SQLiteDatabase9lastErrorEv` | `96iwUEq1jKY` |

## remove | `_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv` | `QZg17ZNOwtc` |

## remove | `_ZN7WebCore14SQLiteDatabase27turnOnIncrementalAutoVacuumEv` | `DfZB2uu86Gw` |

## remove | `_ZN7WebCore14SQLiteDatabase20setCollationFunctionERKN3WTF6StringEONS1_8FunctionIFiiPKviS7_EEE` | `7uInq-rV0oc` |

## remove | `_ZN7WebCore14SQLiteDatabase29setIsDatabaseOpeningForbiddenEb` | `-OrgQoBbx8M` |

## remove | `_ZN7WebCore15SQLiteStatementD1Ev` | `C-lrlIiUeZs` |

## remove | `_ZN7WebCore15SQLiteStatementD2Ev` | `6W+ovJY9kVo` |

## remove | `_ZN7WebCore15SQLiteStatement4stepEv` | `ZO39wK-KwXE` |

## remove | `_ZN7WebCore15SQLiteStatement5resetEv` | `fZbmrDb2Wl4` |

## remove | `_ZN7WebCore15SQLiteStatement14executeCommandEv` | `pgfsfmVnZ80` |

## remove | `_ZN7WebCore15SQLiteStatement8bindBlobEiRKN3WTF6StringE` | `L3OgkCd-Wws` |

## remove | `_ZN7WebCore15SQLiteStatement7bindIntEii` | `su0zXmF7Cto` |

## remove | `_ZN7WebCore15SQLiteStatement9bindInt64Eil` | `3hRFngR3flU` |

## remove | `_ZN7WebCore15SQLiteStatement10bindDoubleEid` | `PVBHZjcjzo0` |

## remove | `_ZN7WebCore15SQLiteStatement8bindNullEi` | `1HJmJsAhIVU` |

## remove | `_ZNK7WebCore15SQLiteStatement18bindParameterCountEv` | `8tE06sxxee8` |

## remove | `_ZN7WebCore15SQLiteStatement22isColumnDeclaredAsBlobEi` | `wYCxExkKxYA` |

## remove | `_ZN7WebCore17SQLiteTransactionC1ERNS_14SQLiteDatabaseEb` | `2yorpzmHA-w` |

## remove | `_ZN7WebCore17SQLiteTransactionC2ERNS_14SQLiteDatabaseEb` | `81MD+QADYBA` |

## remove | `_ZN7WebCore17SQLiteTransactionD1Ev` | `r-3VcPGTSfs` |

## remove | `_ZN7WebCore17SQLiteTransactionD2Ev` | `Myvbbodp9dU` |

## remove | `_ZN7WebCore17SQLiteTransaction5beginEv` | `KfpqmLwti6U` |

## remove | `_ZN7WebCore17SQLiteTransaction6commitEv` | `qpSe9uajx7c` |

## remove | `_ZN7WebCore17SQLiteTransaction8rollbackEv` | `cM1AOABqE-Y` |

## remove | `_ZN7WebCore29SQLiteStatementAutoResetScopeC1EOS0_` | `EiOhI0xdnqs` |

## remove | `_ZN7WebCore29SQLiteStatementAutoResetScopeC2EOS0_` | `I6BK+r-jqhk` |

## remove | `_ZN7WebCore29SQLiteStatementAutoResetScopeaSEOS0_` | `5Z7z4XjqxcA` |

## remove | `_ZN7WebCore29SQLiteStatementAutoResetScopeD1Ev` | `-D-Dsk4CZXA` |

## remove | `_ZN7WebCore29SQLiteStatementAutoResetScopeD2Ev` | `W4vhaG7GsbI` |
