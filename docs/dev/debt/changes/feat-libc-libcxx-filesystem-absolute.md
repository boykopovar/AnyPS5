# libc

## add Functional

- [std::__1::__fs::filesystem::__absolute](../../../core/libs/prx/libc/src/LibcxxFilesystemAbsolute.cpp) (libc) - throws `NotImplemented` for a non-null `error_code*` and, with a null one, when guest `getcwd` fails: `error_code::clear` needs libc++ `system_category`, the error needs libc++ `generic_category` and `filesystem_error`, and none is in libc. The null-`error_code` success path follows [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/filesystem/operations.cpp) `__do_absolute` and `path::operator/=`, including its string growth for the result capacity; the console provider and libc++ version are unverified. Imported by Azahar in RetroArch (PPSA99169, #5927).
