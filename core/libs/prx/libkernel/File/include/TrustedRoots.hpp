#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_TRUSTEDROOTS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_TRUSTEDROOTS_HPP

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <string>

extern "C" std::string ExportHostRoots_nid_no_patch(HCERTSTORE roots, HCERTSTORE disallowed);
extern "C" std::string HostTrustedTlsRoots_nid_no_patch();

#endif
