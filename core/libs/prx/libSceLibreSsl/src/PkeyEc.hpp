/* Derived from LibreSSL 2.5.4 (openbsd 2e2c612066aa993717528a0249c15c51128d61cb): src/lib/libcrypto/evp/p_lib.c (v 1.17) L211-258;
 * src/lib/libcrypto/ec/ec_ameth.c (v 1.10) L186-213. */
#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSsl {

bool PkeyAssignEcKey_nid_no_patch(void* value, EcKey* key);

}
