# libSceNKWebKitRequirements

## change [libpsl exports]

- [libpsl exports](../../../core/libs/prx/libSceNKWebKitRequirements/Psl.cpp) have no console verification. The 22 exports run libpsl 0.21.5 from `3rdparty/libpsl`; its built-in table is generated at build time from the pinned `list/public_suffix_list.dat`, so `psl_builtin` answers follow that snapshot, not the console's. File times and checksums returned by `psl_builtin_file_time` and `psl_builtin_sha1sum` describe that snapshot. `psl_is_cookie_domain_acceptable` is libpsl's code with the guest `inet_pton`, so it tells IP hosts from names as libpsl's non-Windows build does on every host; libpsl's Windows build uses `WSAStringToAddressW`, which needs `WSAStartup` and also takes forms such as `1.2.3`. Closes when: compared with the console.
