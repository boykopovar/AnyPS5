#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

bool ThrowsNotImplemented(const char* name, int (*call)()) {
    try {
        (void)call();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        const std::string expected = std::string(name) + " not implemented";
        if (message == expected) return true;
        std::fprintf(stderr, "%s threw unexpected error: %s\n", name, error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name);
    return false;
}

}

int main() {
    if (sceFiosIsInitialized() != 0) {
        std::fprintf(stderr, "sceFiosIsInitialized before initialize did not report false\n");
        return 1;
    }
    if (sceFiosIsIdle() != 0) {
        std::fprintf(stderr, "sceFiosIsIdle before initialize did not report false\n");
        return 1;
    }
    if (sceFiosIsSuspended() != 0 || sceFiosGetSuspendCount() != 0) {
        std::fprintf(stderr, "Fios2 suspend state was not clear before initialize\n");
        return 1;
    }
    if (sceFiosDLLInitialize() != 0 || sceFiosInitialize() != 0) {
        std::fprintf(stderr, "Fios2 module or library initialize failed\n");
        return 1;
    }
    if (sceFiosIsInitialized() != 1) {
        std::fprintf(stderr, "sceFiosIsInitialized after initialize did not report true\n");
        return 1;
    }
    if (sceFiosIsIdle() != 1) {
        std::fprintf(stderr, "sceFiosIsIdle after initialize did not report idle\n");
        return 1;
    }
    if (sceFiosSuspend() != 0 || sceFiosGetSuspendCount() != 1 || sceFiosIsSuspended() != 1) {
        std::fprintf(stderr, "Fios2 suspend did not raise suspend depth\n");
        return 1;
    }
    if (sceFiosResume() != 0 || sceFiosGetSuspendCount() != 0 || sceFiosIsSuspended() != 0) {
        std::fprintf(stderr, "Fios2 resume did not clear suspend depth\n");
        return 1;
    }
    if (sceFiosCancelAllOps() != 0 || sceFiosCloseAllFiles() != 0) {
        std::fprintf(stderr, "Fios2 cancel-all or close-all failed\n");
        return 1;
    }
    if (sceFiosSuspend() != 0 || sceFiosShutdownAndCancelOps() != 0) {
        std::fprintf(stderr, "Fios2 shutdown-and-cancel failed\n");
        return 1;
    }
    if (sceFiosGetSuspendCount() != 0 || sceFiosIsSuspended() != 0) {
        std::fprintf(stderr, "Fios2 shutdown-and-cancel did not clear suspend depth\n");
        return 1;
    }
    if (!ThrowsNotImplemented("sceFiosArchiveMount", sceFiosArchiveMount)) {
        return 1;
    }
    if (sceFiosTerminate() != 0 || sceFiosDLLTerminate() != 0) {
        std::fprintf(stderr, "Fios2 terminate failed\n");
        return 1;
    }
    if (sceFiosIsInitialized() != 0) {
        std::fprintf(stderr, "sceFiosIsInitialized after terminate did not report false\n");
        return 1;
    }
    return 0;
}
