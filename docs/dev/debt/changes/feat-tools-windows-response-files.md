# TechnicalDebt

## change do not support Windows-style response-file quoting

- [Quick syntax checks](../../tools/check_compile.py) do not support response files of cl-mode drivers (`cl`, `clang-cl`, `--driver-mode=cl`), whose option grammar they do not model. Windows-quoted response files (`--rsp-quoting=windows`) reject NUL characters and text that is neither UTF-8 nor UTF-16 with a byte order mark, and an `--rsp-quoting=` value other than `posix` or `windows` is rejected, although Clang ignores it. GNU response files reject empty options, byte order marks, quoted CRLF, Windows text-mode EOF markers and escapes whose meanings differ between GCC and Clang, including backslashes in single quotes, most backslashes in double quotes and escaped line endings. Supply expanded compile arguments for those forms; the Ninja fallback uses `ninja -t compdb -x`.
