#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct sqlite3;
struct sqlite3_stmt;

namespace NKWebKit::WebCoreAbi {

inline constexpr int SQLiteError = 1;

struct SQLiteDatabase {
    sqlite3* db = nullptr;
    int pageSize = -1;
    bool transactionInProgress = false;
    bool useWAL = false;
    std::uint8_t authorizerLock = 0;
    void* authorizer = nullptr;
    std::uint8_t lockingMutex = 0;
    void* openingThread = nullptr;
    std::uint8_t databaseClosingMutex = 0;
    int openError = SQLiteError;
    WtfAbi::CString openErrorMessage;
    int lastChangesCount = 0;
};

struct SQLiteStatement {
    SQLiteDatabase* database;
    WtfAbi::String query;
    sqlite3_stmt* statement;
};

struct SQLiteTransaction {
    SQLiteDatabase* db;
    bool inProgress;
    bool readOnly;
};

struct SQLiteStatementAutoResetScope {
    SQLiteStatement* statement;
};

struct SQLValue {
    union {
        std::nullptr_t null;
        WtfAbi::StringImpl* string;
        double number;
    };
    signed char index;
};

struct CollationCallable {
    void* const* vtable;
};

struct CollationFunction {
    CollationCallable* callableWrapper;
};

inline constexpr unsigned CollationDeletingDestructorSlot = 1;
inline constexpr unsigned CollationCallSlot = 2;

static_assert(sizeof(SQLiteDatabase) == 64);
static_assert(alignof(SQLiteDatabase) == 8);
static_assert(offsetof(SQLiteDatabase, db) == 0);
static_assert(offsetof(SQLiteDatabase, pageSize) == 8);
static_assert(offsetof(SQLiteDatabase, transactionInProgress) == 12);
static_assert(offsetof(SQLiteDatabase, useWAL) == 13);
static_assert(offsetof(SQLiteDatabase, authorizerLock) == 14);
static_assert(offsetof(SQLiteDatabase, authorizer) == 16);
static_assert(offsetof(SQLiteDatabase, lockingMutex) == 24);
static_assert(offsetof(SQLiteDatabase, openingThread) == 32);
static_assert(offsetof(SQLiteDatabase, databaseClosingMutex) == 40);
static_assert(offsetof(SQLiteDatabase, openError) == 44);
static_assert(offsetof(SQLiteDatabase, openErrorMessage) == 48);
static_assert(offsetof(SQLiteDatabase, lastChangesCount) == 56);
static_assert(sizeof(SQLiteStatement) == 24);
static_assert(offsetof(SQLiteStatement, database) == 0);
static_assert(offsetof(SQLiteStatement, query) == 8);
static_assert(offsetof(SQLiteStatement, statement) == 16);
static_assert(sizeof(SQLiteTransaction) == 16);
static_assert(offsetof(SQLiteTransaction, db) == 0);
static_assert(offsetof(SQLiteTransaction, inProgress) == 8);
static_assert(offsetof(SQLiteTransaction, readOnly) == 9);
static_assert(sizeof(SQLiteStatementAutoResetScope) == 8);
static_assert(sizeof(SQLValue) == 16);
static_assert(offsetof(SQLValue, string) == 0);
static_assert(offsetof(SQLValue, number) == 0);
static_assert(offsetof(SQLValue, index) == 8);
static_assert(sizeof(CollationFunction) == 8);
static_assert(std::is_standard_layout_v<SQLiteDatabase>);
static_assert(std::is_standard_layout_v<SQLiteStatement>);

}  // namespace NKWebKit::WebCoreAbi
