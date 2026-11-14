#pragma once

#include <cstddef>

namespace NKWebKit::SQLiteAbi {

struct SQLiteDatabaseTrackerClient {
    void* const* vtable;
};

inline constexpr unsigned WillBeginFirstTransactionSlot = 2;
inline constexpr unsigned DidFinishLastTransactionSlot = 3;

static_assert(sizeof(SQLiteDatabaseTrackerClient) == 8);
static_assert(offsetof(SQLiteDatabaseTrackerClient, vtable) == 0);

}
