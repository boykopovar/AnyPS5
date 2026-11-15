#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSsl {

LibreSslX509Name* X509NameDup_nid_no_patch(const LibreSslX509Name* name);
void X509NameFree_nid_no_patch(LibreSslX509Name* name);
LibreSslStack* X509NameStackNew_nid_no_patch();
int X509NameStackPush_nid_no_patch(LibreSslStack* stack, LibreSslX509Name* name);

}
