#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdio>
#include <cstring>

extern "C" void APS5_VABI ERR_print_errors_cb_nid_postfix(LibreSsl3ErrorCallback callback, void* userData) {
    if (!callback && ERR_peek_error_nid_postfix() != 0) NotImplemented_nid_no_patch(__func__);
    const auto threadId = reinterpret_cast<std::uintptr_t>(pthread_self_nid_postfix());

    for (;;) {
        const char* file = nullptr;
        int line = 0;
        const auto error = LibreSslPopError_nid_no_patch(&file, &line);
        if (error == 0) return;

        char description[256]{};
        ERR_error_string_n_nid_postfix(error, description, sizeof(description));
        char report[4096]{};
        std::snprintf(report, sizeof(report), "%llu:%s:%s:%d:\n",
            static_cast<unsigned long long>(threadId), description, file ? file : "NA", file ? line : 0);
        if (callback(report, std::strlen(report), userData) <= 0) return;
    }
}
