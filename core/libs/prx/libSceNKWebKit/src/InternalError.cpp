#include "prx/libSceNKWebKit/include/InternalErrorExports.hpp"
#include "prx/libSceNKWebKit/include/LocalizedStringsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ResourceError;
using NKWebKit::WebCoreAbi::ResourceErrorType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

constexpr char ErrorDomain[] = "WebKitErrorDomain";
constexpr int InternalErrorCode = 300;

}

extern "C" {

ResourceError APS5_VABI _ZN7WebCore13internalErrorERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url) {
    if (!url) throw std::invalid_argument("WebCore::internalError: null WTF::URL");
    const int savedErrno = errno;
    auto fail = [&](const char* what) {
        errno = savedErrno;
        throw std::invalid_argument(std::string("WebCore::internalError: ") + what);
    };
    URL guestUrl{};
    if (!GuestCopyIn_nid_no_patch(&guestUrl, url, sizeof(guestUrl))) fail("unreadable WTF::URL");
    String domain = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(ErrorDomain), sizeof(ErrorDomain) - 1);
    String description = _ZN7WebCore15localizedStringEPKc("WebKit encountered an internal error");
    StringImpl guestImpl{};
    if (guestUrl.impl) {
        if (!GuestCopyIn_nid_no_patch(&guestImpl, guestUrl.impl, sizeof(guestImpl))) fail("unreadable WTF::StringImpl");
        guestImpl.refCount += 2;
        if (!GuestCopyOut_nid_no_patch(guestUrl.impl, &guestImpl, sizeof(guestImpl))) fail("unwritable WTF::StringImpl");
    }
    ResourceError error{};
    error.failingURL = guestUrl;
    error.errorCode = InternalErrorCode;
    error.type = ResourceErrorType::General;
    error.domain = std::exchange(domain.impl, nullptr);
    error.localizedDescription = std::exchange(description.impl, nullptr);
    errno = savedErrno;
    return error;
}

}
