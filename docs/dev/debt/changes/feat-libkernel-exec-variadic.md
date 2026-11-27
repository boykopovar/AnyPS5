# libkernel

## add Functional

- [execl, execle and execlp](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - collect the variadic arguments as POSIX.1-2008 specifies, reading them in place, then call `execv`, `execve` or `execvp`. On Windows the arguments are read with the System V layout that guest variadic functions use, as the libc formatting code does. The console's process model and the variadic argument layout of guest callers are unverified. The test checks the failure paths, where argument and environment layout decide the reported errno.
