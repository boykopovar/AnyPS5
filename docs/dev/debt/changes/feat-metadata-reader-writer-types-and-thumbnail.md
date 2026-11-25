# libSceMetadataReaderWriter

## change 6 entries present in both
- [Additional callable exports](../../../core/libs/prx/libSceMetadataReaderWriter/Export.cpp) - 1 entry (`ElB1bNKymPg`) present in both version-labelled images is an import-resolution declaration only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; the parameterless `int` declaration is a placeholder that throws through `NotImplemented_nid_no_patch`.

## remove | `Unknown` | `2oFr2hbrkRs` |

## remove | `Unknown` | `GCOzTsybYr0` |

## remove | `Unknown` | `Ly4V1dsyKCA` |

## remove | `Unknown` | `YeIbFGmyUVM` |

## remove | `Unknown` | `ckVz4iaNj4E` |

## add Unknown function info
- [`ParserInterface` copy constructors, `Value::operator=`, `Metadata::getFieldCount`, and Thumbnail helpers (`Ly4V1dsyKCA`, `ckVz4iaNj4E`, `2oFr2hbrkRs`, `YeIbFGmyUVM`, `GCOzTsybYr0`)](../../../core/libs/prx/libSceMetadataReaderWriter/Export.cpp) - implemented from firmware 13.60 `libSceMetadataReaderWriter.sprx` disassembly (`0xa8f0`, `0x1cf0`, `0x5410`, `0xc980`, `0xc9a0`, `0xca00`); null/unmapped pointer throws are host-safety fallbacks unmeasured on console.
