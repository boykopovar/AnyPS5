#ifndef CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_SQLITE_HPP
#define CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_SQLITE_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_value;
struct sqlite3_context;

namespace NKWebKit {

using GuestSqliteDestructor = void(APS5_VABI*)(void*);
using GuestSqliteBusyHandler = int(APS5_VABI*)(void*, int);
using GuestSqliteAuthorizer = int(APS5_VABI*)(void*, int, const char*, const char*, const char*, const char*);
using GuestSqliteWalHook = int(APS5_VABI*)(void*, sqlite3*, const char*, int);
using GuestSqliteCollation = int(APS5_VABI*)(void*, int, const void*, int, const void*);
using GuestSqliteFunction = void(APS5_VABI*)(sqlite3_context*, int, sqlite3_value**);
using GuestSqliteFinal = void(APS5_VABI*)(sqlite3_context*);

} // namespace NKWebKit

#endif
