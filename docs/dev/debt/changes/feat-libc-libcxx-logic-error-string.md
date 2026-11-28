# libc

## add Functional

- [libc++ string-taking logic_error constructors](../../../core/libs/prx/libc/src/exception/LibcxxLogicErrorString.cpp) (libc) supply the complete-object and base-object constructors for logic_error, invalid_argument, out_of_range, length_error, and domain_error. They read the observed libc++ string layout and pass the C-string diagnostic to the existing owned exception storage. The firmware provider and full title behavior remain unmeasured.
