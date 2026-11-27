# libSceLibreSsl

## change [OPENSSL_add_all_algorithms_noconf, OPENSSL_load_builtin_modules, CONF_modules_free]

- [OPENSSL_add_all_algorithms_noconf, OPENSSL_load_builtin_modules, CONF_modules_free](../../../core/libs/prx/libSceLibreSsl/src/Conf.cpp) (libSceLibreSsl) - unverified on a PS5. `OPENSSL_add_all_algorithms_noconf`, in `Export.cpp`, still returns without filling cipher/digest name tables. `OPENSSL_load_builtin_modules` registers `oid_section` and `engines`; `CONF_modules_free` clears loaded module state and added OIDs. Other built-in modules and module callbacks are not implemented. LibreSSL 2.5.4 [conf_mod.c](https://github.com/libressl/openbsd/blob/2e2c612066aa993717528a0249c15c51128d61cb/src/lib/libcrypto/conf/conf_mod.c).

## change [CONF_modules_load_file]

- [CONF_modules_load_file](../../../core/libs/prx/libSceLibreSsl/src/Conf.cpp) (libSceLibreSsl) - not checked on the console. Supports common LibreSSL 2.5.4 configuration syntax, duplicate key replacement, variable lookup with `default` fallback, `oid_section`, and `engines` dispatch. In the pinned LibreSSL version, `$ENV::name` resolves the `ENV` configuration section and then `default`; it does not read the process environment. Section and key names accept the 2.5.4 default character classes, including its punctuation; other character tables are not supported. The parser and error reporting are not complete for all inputs. Dynamic module loading remains unsupported and returns DSO errors. A null file name uses `/etc/ssl/openssl.cnf`.
