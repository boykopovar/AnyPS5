# libcurl

## add Functional

- [`curl_mime_filedata`](../../../core/libs/prx/libcurl/Export.cpp) - whether the console's curl takes the part's file name from `basename(3)` or from curl 7.81.0's own `Curl_basename`, which also splits at `\`, is unverified. The guest path is resolved like `fopen`, curl opens the host path, and the file name is the guest libc `basename` of the guest path; a path that does not resolve gives a file part that cannot be opened and `CURLE_READ_ERROR`, as a path that fails `stat` does. On Windows the backend opens files through the ANSI API, so a host path outside the active code page cannot be read. `CURLFORM_FILE`, `CURLFORM_FILECONTENT` and the options that name files, such as `CURLOPT_CAINFO` and `CURLOPT_COOKIEFILE`, still take host paths.
