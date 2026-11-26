# libScePosixForWebKit

## add Unknown function info

- [getpwuid_r and getpwnam_r](../../../../core/libs/prx/libScePosixForWebKit/UserDatabase.cpp) (libScePosixForWebKit) - they report only the root account, the same account as `getpwuid`, whose console fields are not verified. The strings are copied into the caller's buffer in the order of the root account (name, password, class, gecos, directory, shell), and a buffer under 12 bytes returns `ERANGE`. A name that is not root, or longer than 4096 bytes, gives a null result with status 0, as POSIX requires for a missing entry.
