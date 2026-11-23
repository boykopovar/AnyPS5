# libc

## add Functional

- [regcomp, regexec, regfree, regerror, glob, globfree, wordexp, wordfree, catopen, catgets, catclose, nl_langinfo, strptime, fmtmsg, confstr, uname and gethostid](../../../core/libs/prx/libc/src/UnsupportedStandard.cpp) (libc) - throw `NotImplemented`. Their console ABI structures (`regex_t`, `glob_t`, `wordexp_t`, catalog and `tm` layouts) and results are unverified, so none is guessed. A title that imports one now loads, and a call reports the missing function.
