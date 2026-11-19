# libcurl

## change [Push ownership]

- [Push ownership](../../../core/libs/prx/libcurl/Export.cpp) - libcurl closes an accepted push whose add to the multi fails, without a notification; the adapter releases that handle's copied callback context when libcurl hands the handle's address to a new easy handle. No title is known to use HTTP/2 server push. Closes when: native easy-handle destruction notifies the adapter for internally rejected pushes.
