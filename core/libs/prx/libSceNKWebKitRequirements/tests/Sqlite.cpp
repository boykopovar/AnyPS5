#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void AssertTrue(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed: %s\n", message);
        std::abort();
    }
}

void AssertEqual(long long actual, long long expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "Assertion failed: %s (actual %lld, expected %lld)\n", message, actual, expected);
        std::abort();
    }
}

void AssertText(const char* actual, const char* expected, const char* message) {
    if (!actual || std::strcmp(actual, expected) != 0) {
        std::fprintf(stderr, "Assertion failed: %s (actual '%s', expected '%s')\n", message, actual ? actual : "(null)", expected);
        std::abort();
    }
}

bool EqualsUtf16(const void* actual, const char16_t* expected) {
    if (!actual) return false;
    const auto* text = static_cast<const char16_t*>(actual);
    std::size_t index = 0;
    for (; expected[index]; ++index) {
        if (text[index] != expected[index]) return false;
    }
    return text[index] == 0;
}

std::size_t Utf16Bytes(const char16_t* text) {
    std::size_t length = 0;
    while (text[length]) ++length;
    return length * sizeof(char16_t);
}

sqlite3* OpenMemory() {
    sqlite3* db = nullptr;
    AssertEqual(sqlite3_initialize_nid_postfix(), SQLITE_OK, "sqlite3_initialize");
    AssertEqual(sqlite3_open_v2_nid_postfix(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr), SQLITE_OK, "open in-memory database");
    AssertTrue(db != nullptr, "open returned a null handle");
    return db;
}

sqlite3_stmt* Prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    const char* tail = nullptr;
    const int result = sqlite3_prepare_v2_nid_postfix(db, sql, -1, &stmt, &tail);
    if (result != SQLITE_OK) {
        std::fprintf(stderr, "prepare failed for '%s': %s\n", sql, sqlite3_errmsg_nid_postfix(db));
        std::abort();
    }
    return stmt;
}

void Exec(sqlite3* db, const char* sql) {
    sqlite3_stmt* stmt = Prepare(db, sql);
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_DONE, sql);
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize after exec");
}

int g_destructorCalls = 0;
const void* g_destructorArgument = nullptr;

void APS5_VABI CountingDestructor(void* data) {
    ++g_destructorCalls;
    g_destructorArgument = data;
}

void TestRoundTrip() {
    sqlite3* db = OpenMemory();
    AssertEqual(sqlite3_get_autocommit_nid_postfix(db), 1, "autocommit is on outside a transaction");
    Exec(db, "CREATE TABLE items(id INTEGER PRIMARY KEY, name TEXT NOT NULL, price REAL, quantity INTEGER, payload BLOB, big INTEGER, label TEXT)");

    sqlite3_stmt* insert = Prepare(db, "INSERT INTO items(id, name, price, quantity, payload, big, label) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    AssertEqual(sqlite3_bind_parameter_count_nid_postfix(insert), 7, "bind parameter count");
    AssertText(sqlite3_sql_nid_postfix(insert), "INSERT INTO items(id, name, price, quantity, payload, big, label) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)", "statement text");

    const unsigned char blob[] = {0x00, 0x01, 0xfe, 0xff, 0x00, 0x7f};
    const char16_t name[] = u"café €";
    AssertEqual(sqlite3_bind_int_nid_postfix(insert, 1, 7), SQLITE_OK, "bind id");
    AssertEqual(sqlite3_bind_text16_nid_postfix(insert, 2, name, static_cast<int>(Utf16Bytes(name)), reinterpret_cast<NKWebKit::GuestSqliteDestructor>(static_cast<std::intptr_t>(-1))), SQLITE_OK, "bind text16 transient");
    AssertEqual(sqlite3_bind_double_nid_postfix(insert, 3, 12.5), SQLITE_OK, "bind double");
    AssertEqual(sqlite3_bind_int_nid_postfix(insert, 4, -42), SQLITE_OK, "bind int");
    AssertEqual(sqlite3_bind_blob_nid_postfix(insert, 5, blob, sizeof(blob), nullptr), SQLITE_OK, "bind blob static");
    AssertEqual(sqlite3_bind_int64_nid_postfix(insert, 6, 0x123456789abcLL), SQLITE_OK, "bind int64");
    AssertEqual(sqlite3_bind_null_nid_postfix(insert, 7), SQLITE_OK, "bind null");
    AssertEqual(sqlite3_bind_int_nid_postfix(insert, 8, 1), SQLITE_RANGE, "bind past the last parameter");
    AssertEqual(sqlite3_bind_int_nid_postfix(insert, 0, 1), SQLITE_RANGE, "bind index zero");
    AssertEqual(sqlite3_step_nid_postfix(insert), SQLITE_DONE, "insert step");
    AssertEqual(sqlite3_data_count_nid_postfix(insert), 0, "no data after an insert");
    AssertEqual(sqlite3_last_insert_rowid_nid_postfix(db), 7, "last insert rowid");
    AssertEqual(sqlite3_total_changes_nid_postfix(db), 1, "total changes");
    AssertEqual(sqlite3_reset_nid_postfix(insert), SQLITE_OK, "reset");
    AssertEqual(sqlite3_clear_bindings_nid_postfix(insert), SQLITE_OK, "clear bindings");
    AssertEqual(sqlite3_finalize_nid_postfix(insert), SQLITE_OK, "finalize insert");

    sqlite3_stmt* select = Prepare(db, "SELECT id, name, price, quantity, payload, big, label FROM items WHERE id = 7");
    AssertEqual(sqlite3_stmt_busy_nid_postfix(select), 0, "statement is idle before stepping");
    AssertEqual(sqlite3_step_nid_postfix(select), SQLITE_ROW, "select step");
    AssertEqual(sqlite3_stmt_busy_nid_postfix(select), 1, "statement is busy on a row");
    AssertEqual(sqlite3_data_count_nid_postfix(select), 7, "data count");
    AssertEqual(sqlite3_column_type_nid_postfix(select, 0), SQLITE_INTEGER, "id type");
    AssertEqual(sqlite3_column_type_nid_postfix(select, 1), SQLITE_TEXT, "name type");
    AssertEqual(sqlite3_column_type_nid_postfix(select, 2), SQLITE_FLOAT, "price type");
    AssertEqual(sqlite3_column_type_nid_postfix(select, 4), SQLITE_BLOB, "payload type");
    AssertEqual(sqlite3_column_type_nid_postfix(select, 6), SQLITE_NULL, "label type");
    AssertEqual(sqlite3_column_int_nid_postfix(select, 0), 7, "id value");
    AssertEqual(sqlite3_column_int64_nid_postfix(select, 5), 0x123456789abcLL, "int64 value");
    AssertEqual(sqlite3_column_int_nid_postfix(select, 3), -42, "int value");
    AssertTrue(sqlite3_column_double_nid_postfix(select, 2) == 12.5, "double value");
    AssertTrue(EqualsUtf16(sqlite3_column_text16_nid_postfix(select, 1), name), "text16 value");
    AssertEqual(sqlite3_column_bytes16_nid_postfix(select, 1), static_cast<long long>(Utf16Bytes(name)), "text16 byte length");
    AssertEqual(sqlite3_column_bytes_nid_postfix(select, 4), sizeof(blob), "blob byte length");
    AssertTrue(std::memcmp(sqlite3_column_blob_nid_postfix(select, 4), blob, sizeof(blob)) == 0, "blob bytes");
    AssertTrue(EqualsUtf16(sqlite3_column_name16_nid_postfix(select, 2), u"price"), "column name16");
    AssertText(sqlite3_column_decltype_nid_postfix(select, 4), "BLOB", "declared type");
    AssertText(sqlite3_column_decltype_nid_postfix(select, 0), "INTEGER", "declared integer type");
    sqlite3_value* value = sqlite3_column_value_nid_postfix(select, 2);
    AssertTrue(value != nullptr, "column value handle");
    AssertEqual(sqlite3_value_type_nid_postfix(value), SQLITE_FLOAT, "value type");
    AssertTrue(sqlite3_value_double_nid_postfix(value) == 12.5, "value double");
    AssertEqual(sqlite3_step_nid_postfix(select), SQLITE_DONE, "select done");
    AssertEqual(sqlite3_data_count_nid_postfix(select), 0, "no data after the last row");
    AssertEqual(sqlite3_next_stmt_nid_postfix(db, nullptr) == select, 1, "next_stmt lists the open statement");
    AssertTrue(sqlite3_next_stmt_nid_postfix(db, select) == nullptr, "next_stmt ends the list");
    AssertEqual(sqlite3_finalize_nid_postfix(select), SQLITE_OK, "finalize select");
    AssertTrue(sqlite3_next_stmt_nid_postfix(db, nullptr) == nullptr, "no statement left");

    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close");
}

void TestErrors() {
    sqlite3* db = OpenMemory();
    Exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    Exec(db, "INSERT INTO t VALUES(1, 'one')");

    sqlite3_stmt* stmt = nullptr;
    const char* tail = nullptr;
    AssertEqual(sqlite3_prepare_v2_nid_postfix(db, "SELEC nonsense", -1, &stmt, &tail), SQLITE_ERROR, "syntax error code");
    AssertTrue(stmt == nullptr, "no statement after a syntax error");
    AssertEqual(sqlite3_errcode_nid_postfix(db), SQLITE_ERROR, "errcode after syntax error");
    AssertText(sqlite3_errmsg_nid_postfix(db), "near \"SELEC\": syntax error", "errmsg after syntax error");

    AssertEqual(sqlite3_prepare_v2_nid_postfix(db, "SELECT * FROM missing", -1, &stmt, &tail), SQLITE_ERROR, "missing table code");
    AssertText(sqlite3_errmsg_nid_postfix(db), "no such table: missing", "missing table message");

    const char* twoStatements = "SELECT 1; SELECT 2";
    stmt = Prepare(db, twoStatements);
    AssertTrue(stmt != nullptr, "first statement prepared");
    AssertTrue(sqlite3_finalize_nid_postfix(stmt) == SQLITE_OK, "finalize first statement");
    AssertEqual(sqlite3_prepare_v2_nid_postfix(db, twoStatements, -1, &stmt, &tail), SQLITE_OK, "prepare with tail");
    AssertText(tail, " SELECT 2", "tail points at the second statement");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize tail statement");

    stmt = Prepare(db, "INSERT INTO t VALUES(1, 'dup')");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_CONSTRAINT, "primary key violation code");
    AssertEqual(sqlite3_reset_nid_postfix(stmt), SQLITE_CONSTRAINT, "reset returns the step error");
    AssertEqual(sqlite3_extended_result_codes_nid_postfix(db, 1), SQLITE_OK, "enable extended result codes");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_CONSTRAINT_PRIMARYKEY, "extended primary key code");
    AssertEqual(sqlite3_errcode_nid_postfix(db), SQLITE_CONSTRAINT_PRIMARYKEY, "errcode extended");
    AssertText(sqlite3_errmsg_nid_postfix(db), "UNIQUE constraint failed: t.id", "constraint message");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_CONSTRAINT_PRIMARYKEY, "finalize returns the error");

    stmt = Prepare(db, "INSERT INTO t VALUES(2, NULL)");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_CONSTRAINT_NOTNULL, "not null violation code");
    AssertText(sqlite3_errmsg_nid_postfix(db), "NOT NULL constraint failed: t.name", "not null message");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_CONSTRAINT_NOTNULL, "finalize not null");

    AssertText(sqlite3_errstr_nid_postfix(SQLITE_OK), "not an error", "errstr ok");
    AssertText(sqlite3_errstr_nid_postfix(SQLITE_BUSY), "database is locked", "errstr busy");
    AssertText(sqlite3_errstr_nid_postfix(SQLITE_RANGE), "column index out of range", "errstr range");
    AssertText(sqlite3_errstr_nid_postfix(SQLITE_ROW), "another row available", "errstr row");
    AssertText(sqlite3_errstr_nid_postfix(SQLITE_DONE), "no more rows available", "errstr done");

    Exec(db, "BEGIN");
    AssertEqual(sqlite3_get_autocommit_nid_postfix(db), 0, "inside a transaction");
    Exec(db, "ROLLBACK");
    AssertEqual(sqlite3_get_autocommit_nid_postfix(db), 1, "after rollback");

    stmt = Prepare(db, "SELECT 1");
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_BUSY, "close with an open statement");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize before closing");
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close after finalize");
}

void TestBindDestructor() {
    sqlite3* db = OpenMemory();
    sqlite3_stmt* stmt = Prepare(db, "SELECT ?1, ?2");
    unsigned char blob[] = {1, 2, 3};
    g_destructorCalls = 0;
    g_destructorArgument = nullptr;
    AssertEqual(sqlite3_bind_blob_nid_postfix(stmt, 1, blob, sizeof(blob), CountingDestructor), SQLITE_OK, "bind blob with destructor");
    AssertEqual(g_destructorCalls, 1, "guest destructor called once");
    AssertTrue(g_destructorArgument == blob, "guest destructor receives the data pointer");
    blob[0] = 9;
    const char16_t text[] = u"ab";
    AssertEqual(sqlite3_bind_text16_nid_postfix(stmt, 2, text, static_cast<int>(Utf16Bytes(text)), CountingDestructor), SQLITE_OK, "bind text16 with destructor");
    AssertEqual(g_destructorCalls, 2, "guest destructor called for text");
    AssertEqual(sqlite3_bind_blob_nid_postfix(stmt, 3, blob, sizeof(blob), CountingDestructor), SQLITE_RANGE, "bind out of range");
    AssertEqual(g_destructorCalls, 3, "guest destructor called when the bind fails");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_ROW, "select bound values");
    AssertEqual(sqlite3_column_bytes_nid_postfix(stmt, 0), 3, "blob copied before destruction");
    AssertEqual(static_cast<const unsigned char*>(sqlite3_column_blob_nid_postfix(stmt, 0))[0], 1, "copy holds the original first byte");
    AssertTrue(EqualsUtf16(sqlite3_column_text16_nid_postfix(stmt, 1), u"ab"), "text copied before destruction");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize");
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close");
}

struct FunctionObservation {
    int calls = 0;
    int argumentCount = 0;
    int firstType = 0;
    double firstValue = 0;
    bool secondIsText = false;
    void* userData = nullptr;
};

FunctionObservation g_observation;
int g_stepCalls = 0;
int g_finalCalls = 0;

void APS5_VABI ObserveFunction(sqlite3_context* context, int count, sqlite3_value** values) {
    ++g_observation.calls;
    g_observation.argumentCount = count;
    g_observation.firstType = sqlite3_value_type_nid_postfix(values[0]);
    g_observation.firstValue = sqlite3_value_double_nid_postfix(values[0]);
    g_observation.secondIsText = EqualsUtf16(sqlite3_value_text16_nid_postfix(values[1]), u"xy");
    g_observation.userData = sqlite3_user_data_nid_postfix(context);
}

void APS5_VABI FailFunction(sqlite3_context* context, int, sqlite3_value**) {
    const char message[] = "custom failure";
    sqlite3_result_error_nid_postfix(context, message, static_cast<int>(sizeof(message) - 1));
}

void APS5_VABI CountStep(sqlite3_context*, int, sqlite3_value**) {
    ++g_stepCalls;
}

void APS5_VABI CountFinal(sqlite3_context*) {
    ++g_finalCalls;
}

void TestFunctions() {
    sqlite3* db = OpenMemory();
    int marker = 0;
    AssertEqual(sqlite3_create_function_nid_postfix(db, "observe", 2, SQLITE_UTF8, &marker, ObserveFunction, nullptr, nullptr), SQLITE_OK, "register scalar function");
    g_observation = {};
    sqlite3_stmt* stmt = Prepare(db, "SELECT observe(2.5, 'xy')");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_ROW, "call scalar function");
    AssertEqual(sqlite3_column_type_nid_postfix(stmt, 0), SQLITE_NULL, "function without a result returns null");
    AssertEqual(g_observation.calls, 1, "function called once");
    AssertEqual(g_observation.argumentCount, 2, "argument count");
    AssertEqual(g_observation.firstType, SQLITE_FLOAT, "first argument type");
    AssertTrue(g_observation.firstValue == 2.5, "first argument value");
    AssertTrue(g_observation.secondIsText, "second argument text16");
    AssertTrue(g_observation.userData == &marker, "user data is the registered pointer");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize scalar");

    AssertEqual(sqlite3_create_function_nid_postfix(db, "fail", 0, SQLITE_UTF8, nullptr, FailFunction, nullptr, nullptr), SQLITE_OK, "register failing function");
    stmt = Prepare(db, "SELECT fail()");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_ERROR, "function error code");
    AssertText(sqlite3_errmsg_nid_postfix(db), "custom failure", "function error message");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_ERROR, "finalize failing function");

    AssertEqual(sqlite3_create_function_nid_postfix(db, "agg", 1, SQLITE_UTF8, nullptr, nullptr, CountStep, CountFinal), SQLITE_OK, "register aggregate");
    Exec(db, "CREATE TABLE n(v)");
    Exec(db, "INSERT INTO n VALUES(1),(2),(3)");
    g_stepCalls = 0;
    g_finalCalls = 0;
    stmt = Prepare(db, "SELECT agg(v) FROM n");
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_ROW, "aggregate row");
    AssertEqual(g_stepCalls, 3, "aggregate step calls");
    AssertEqual(g_finalCalls, 1, "aggregate final calls");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize aggregate");

    AssertEqual(sqlite3_create_function_nid_postfix(db, "bad", 1, SQLITE_UTF8, nullptr, ObserveFunction, CountStep, nullptr), SQLITE_MISUSE, "mixed scalar and aggregate callbacks are refused");
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close");
}

int g_collationDestroyed = 0;
void* g_collationDestroyArgument = nullptr;

int APS5_VABI ReverseCompare(void*, int firstSize, const void* first, int secondSize, const void* second) {
    const int common = firstSize < secondSize ? firstSize : secondSize;
    const int order = std::memcmp(second, first, static_cast<std::size_t>(common));
    if (order != 0) return order;
    return secondSize - firstSize;
}

void APS5_VABI CollationDestroy(void* argument) {
    ++g_collationDestroyed;
    g_collationDestroyArgument = argument;
}

void TestCollation() {
    sqlite3* db = OpenMemory();
    int marker = 0;
    g_collationDestroyed = 0;
    AssertEqual(sqlite3_create_collation_v2_nid_postfix(db, "REVERSE", SQLITE_UTF8, &marker, ReverseCompare, CollationDestroy), SQLITE_OK, "register collation");
    Exec(db, "CREATE TABLE words(w TEXT)");
    Exec(db, "INSERT INTO words VALUES('apple'),('cherry'),('banana')");
    sqlite3_stmt* stmt = Prepare(db, "SELECT w FROM words ORDER BY w COLLATE REVERSE");
    const char* expected[] = {"cherry", "banana", "apple"};
    for (const char* word : expected) {
        AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_ROW, "collation row");
        AssertEqual(sqlite3_column_bytes_nid_postfix(stmt, 0), static_cast<long long>(std::strlen(word)), "collation row length");
        AssertTrue(std::memcmp(sqlite3_column_blob_nid_postfix(stmt, 0), word, std::strlen(word)) == 0, "collation order");
    }
    AssertEqual(sqlite3_step_nid_postfix(stmt), SQLITE_DONE, "collation done");
    AssertEqual(sqlite3_finalize_nid_postfix(stmt), SQLITE_OK, "finalize collation query");
    AssertEqual(g_collationDestroyed, 0, "collation alive while the database is open");
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close");
    AssertEqual(g_collationDestroyed, 1, "collation destroyed on close");
    AssertTrue(g_collationDestroyArgument == &marker, "collation destroy receives the registered argument");
}

int g_authorizerCalls = 0;
void* g_authorizerArgument = nullptr;

int APS5_VABI DenyDrop(void* argument, int action, const char*, const char*, const char*, const char*) {
    ++g_authorizerCalls;
    g_authorizerArgument = argument;
    return action == SQLITE_DROP_TABLE ? SQLITE_DENY : SQLITE_OK;
}

int g_walCalls = 0;

int APS5_VABI CountWal(void*, sqlite3*, const char*, int) {
    ++g_walCalls;
    return SQLITE_OK;
}

int APS5_VABI GiveUpBusy(void*, int) {
    return 0;
}

void TestHooks() {
    sqlite3* db = OpenMemory();
    int marker = 0;
    Exec(db, "CREATE TABLE doomed(x)");
    AssertEqual(sqlite3_set_authorizer_nid_postfix(db, DenyDrop, &marker), SQLITE_OK, "set authorizer");
    sqlite3_stmt* stmt = nullptr;
    const char* tail = nullptr;
    AssertEqual(sqlite3_prepare_v2_nid_postfix(db, "DROP TABLE doomed", -1, &stmt, &tail), SQLITE_AUTH, "authorizer denies the drop");
    AssertTrue(g_authorizerCalls > 0 && g_authorizerArgument == &marker, "authorizer called with its argument");
    AssertText(sqlite3_errmsg_nid_postfix(db), "not authorized", "authorizer message");
    AssertEqual(sqlite3_set_authorizer_nid_postfix(db, nullptr, nullptr), SQLITE_OK, "remove authorizer");
    Exec(db, "DROP TABLE doomed");

    int first = 0;
    int second = 0;
    AssertTrue(sqlite3_wal_hook_nid_postfix(db, CountWal, &first) == nullptr, "no previous wal hook");
    AssertTrue(sqlite3_wal_hook_nid_postfix(db, CountWal, &second) == &first, "previous wal hook argument returned");
    AssertTrue(sqlite3_wal_hook_nid_postfix(db, nullptr, nullptr) == &second, "wal hook removed");
    AssertEqual(g_walCalls, 0, "wal hook never called outside wal mode");

    AssertEqual(sqlite3_busy_handler_nid_postfix(db, GiveUpBusy, &marker), SQLITE_OK, "set busy handler");
    AssertEqual(sqlite3_busy_handler_nid_postfix(db, nullptr, nullptr), SQLITE_OK, "remove busy handler");
    AssertEqual(sqlite3_busy_timeout_nid_postfix(db, 10), SQLITE_OK, "set busy timeout");
    AssertEqual(sqlite3_wal_checkpoint_v2_nid_postfix(db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr), SQLITE_OK, "checkpoint a non-wal database");
    sqlite3_interrupt_nid_postfix(db);
    AssertEqual(sqlite3_close_nid_postfix(db), SQLITE_OK, "close");
}

}

int main() {
    TestRoundTrip();
    TestErrors();
    TestBindDestructor();
    TestFunctions();
    TestCollation();
    TestHooks();
    std::printf("Sqlite tests passed.\n");
    return 0;
}
