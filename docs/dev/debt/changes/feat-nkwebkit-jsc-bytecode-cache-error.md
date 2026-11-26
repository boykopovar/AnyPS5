# libSceNKWebKit

## add Functional

- [JSC::BytecodeCacheError::isValid and its StandardError and WriteError assignments](../../../core/libs/prx/libSceNKWebKit/src/JscBytecodeCacheError.cpp) (libSceNKWebKit) - not compared with a console module; the 96-byte layout in `JscBytecodeCacheErrorExports.hpp` (a `std::variant<ParserError, StandardError, WriteError>` with the 88-byte `ParserError` holding `m_message` at 72 and `m_type` at 84, and a 4-byte libc++ ABI v1 index at 88) follows the pinned headers and is unverified. The low byte of the index is read; `isValid` on a valueless error and any call with an index above 2 other than `0xFF`, a null object or a null argument throw `std::invalid_argument` instead of crashing. Closes when: compared with a console module.

## remove | `_ZNK3JSC18BytecodeCacheError7isValidEv` | `uBoKXflFvyo` |

## remove | `_ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE` | `pUw8oIcCMXk` |

## remove | `_ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE` | `B+VIoA9pLb0` |
