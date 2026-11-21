# libSceNKWebKit

## add Functional

- [WTF::BitVector::dump and WTF::Thread::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfObjectDumps.cpp) (libSceNKWebKit) - not compared with a console module; `%p` output is whatever the stream's `vprintf` makes of it. They follow the pinned [BitVector.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/BitVector.cpp#L276-L280) and [Threading.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Threading.cpp#L473-L476): `BitVector::dump` makes one `print` (`begin`, `printf("%s")`, `end`) per bit of `size()` (63 inline, the rounded word count out of line), `1` or `-`; `Thread::dump` prints `Thread:` and `printf("%p", this)` in one `print`. An empty or deleted hash-table `BitVector` throws instead of reading through its null pointer. Closes when: compared with a console module.

## remove | `_ZNK3WTF9BitVector4dumpERNS_11PrintStreamE` | `o2Z-He35x8w` |

## remove | `_ZNK3WTF6Thread4dumpERNS_11PrintStreamE` | `IEIydcJjkbo` |
