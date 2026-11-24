# libSceLibreSsl

## change [Memory BIO unchecked inputs]

- [Memory BIO unchecked inputs](../../../core/libs/prx/libSceLibreSsl/src/Bio.cpp) throw for null, unreadable or page-straddling methods, unreadable, read-only or page-straddling `BIO`, memory buffer and SSL BIO state records, unmapped or read-only `SSL` / internal callback targets, raw line-buffer pointers, negative raw method write sizes, an overflowing raw line-buffer size, inconsistent read-only buffer lengths, and memory write lengths that overflow size_t. Upstream does not validate these inputs. An unrepresentable string length throws instead of narrowing into an unchecked write.
