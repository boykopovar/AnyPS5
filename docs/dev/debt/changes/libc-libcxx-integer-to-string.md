# libc

## add Functional

- [libc++ integer to_string](../../../core/libs/prx/libc/src/LibcxxIntegerToString.cpp) provides the four integer overloads imported by Azahar (#5927). All results fit in the 24-byte guest short-string representation and use the guest calling convention for returning the result. Full title and console behavior remain unverified.
