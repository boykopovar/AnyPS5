# libScePosixForWebKit

## add Unknown function info

- [getpwent, setpwent and endpwent](../../../core/libs/prx/libScePosixForWebKit/UserDatabase.cpp) (libScePosixForWebKit) - the enumeration returns only the root account that `getpwuid` reports; the console's account list is not known, so other accounts are not enumerated. The position is one process-wide flag, so concurrent enumeration from several threads can see each other's position.
