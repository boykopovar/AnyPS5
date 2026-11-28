/* Derived from lib/formdata.c@801bd5138ce31aa0d906fa4e2eabfc599d74e793 */
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2021, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include "prx/libcurl/src/CurlHost.hpp"
#include "prx/libcurl/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>

extern "C" {
extern curl_free_callback Curl_cfree;
}

void APS5_VABI curl_formfree_nid_postfix(struct guest_curl_httppost* form) {
    while (form) {
        guest_curl_httppost node{};
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&node, form, sizeof(node));
        errno = savedErrno;
        if (!copied) throw std::invalid_argument("Invalid curl_httppost pointer");
        curl_formfree_nid_postfix(node.more);
        if (!(node.flags & CURL_HTTPPOST_PTRNAME)) Curl_cfree(node.name);
        if (!(node.flags & (CURL_HTTPPOST_PTRCONTENTS | CURL_HTTPPOST_BUFFER | CURL_HTTPPOST_CALLBACK)))
            Curl_cfree(node.contents);
        Curl_cfree(node.contenttype);
        Curl_cfree(node.showfilename);
        Curl_cfree(form);
        form = node.next;
    }
}
