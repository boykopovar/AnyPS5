# libc

## add Functional

- [tsearch, tfind, tdelete and twalk](../../../core/libs/prx/libc/src/SearchTree.cpp) (libc) - console behavior is unverified. Implemented from POSIX.1-2008 as an AVL tree whose nodes come from the guest heap and are freed by `tdelete`. The tree stores the key pointers it is given, not copies. When `tdelete` removes the root, it returns the root location, which POSIX leaves unspecified. A null `rootp` returns null, as POSIX requires, and a null action makes `twalk` do nothing. A null key or comparison function, and an unreadable or unwritable root, throw.
