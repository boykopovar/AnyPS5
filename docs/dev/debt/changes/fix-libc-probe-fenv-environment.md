# libc

## add Unknown function info

- [fegetenv, feholdexcept, fesetenv and feupdateenv environment](../../../core/libs/prx/libc/src/Math.cpp) (libc) - the 32-byte environment is written or read with one checked guest copy; an unwritable or unreadable environment throws `std::invalid_argument`, as a null one already does, because whether the console faults or returns nonzero on unmapped memory is unmeasured. `feholdexcept` saves the environment before it changes the floating-point state, and `fesetenv` / `feupdateenv` read it before applying anything, so a throw leaves the state alone.
