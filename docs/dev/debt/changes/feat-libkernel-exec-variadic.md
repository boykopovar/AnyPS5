# libkernel

## add Functional

- [execl, execle and execlp](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - collect the variadic arguments as POSIX.1-2008 specifies, then call `execv`, `execve` or `execvp` with them. The console's process model and the variadic argument layout of guest callers are unverified; the test checks only the failure path for a missing program.
