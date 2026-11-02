#include "prx/libSceNKWebKitRequirements/include/Sqlite.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ERROR = 1;
constexpr int SQLITE_BUSY = 5;
constexpr int SQLITE_INTERRUPT = 9;
constexpr int SQLITE_AUTH = 23;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_CONSTRAINT_UNIQUE = 2067;
constexpr int SQLITE_OPEN_READWRITE = 0x2;
constexpr int SQLITE_OPEN_CREATE = 0x4;
constexpr int SQLITE_UTF8 = 1;
constexpr int SQLITE_UTF16LE = 2;
constexpr int SQLITE_INTEGER = 1;
constexpr int SQLITE_FLOAT = 2;
constexpr int SQLITE_TEXT = 3;
constexpr int SQLITE_BLOB = 4;
constexpr int SQLITE_NULL = 5;
constexpr int SQLITE_DENY = 1;
constexpr int SQLITE_DELETE = 9;
constexpr int SQLITE_CHECKPOINT_PASSIVE = 0;
const auto SQLITE_STATIC = reinterpret_cast<NKWebKit::GuestSqliteDestructor>(std::intptr_t{0});
const auto SQLITE_TRANSIENT = reinterpret_cast<NKWebKit::GuestSqliteDestructor>(std::intptr_t{-1});

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "NKWebKit SQLite assertion failed: %s\n", message);
    std::abort();
}

int destroyedBuffers = 0;
int destroyedCollations = 0;
int functionCalls = 0;
int stepCalls = 0;
int finalCalls = 0;
int busyCalls = 0;
int walCalls = 0;
double lastFunctionValue = 0;
void* lastFunctionApplication = nullptr;
int walFrames = 0;
std::string walSchema;
sqlite3* interruptTarget = nullptr;

void APS5_VABI FreeBuffer(void* buffer) {
    ++destroyedBuffers;
    std::memset(buffer, 0x5A, 8);
    std::free(buffer);
}

int APS5_VABI ReverseCompare(void* argument, int leftSize, const void* left, int rightSize, const void* right) {
    Require(*static_cast<int*>(argument) == 0x51, "collation argument");
    const int common = leftSize < rightSize ? leftSize : rightSize;
    const int order = std::memcmp(left, right, static_cast<std::size_t>(common));
    return order ? -order : rightSize - leftSize;
}

void APS5_VABI DestroyCollation(void* argument) {
    Require(*static_cast<int*>(argument) == 0x51, "collation destroy argument");
    ++destroyedCollations;
}

void APS5_VABI Probe(sqlite3_context* context, int count, sqlite3_value** values) {
    ++functionCalls;
    Require(count == 1, "function argument count");
    lastFunctionApplication = sqlite3_user_data_nid_postfix(context);
    if (sqlite3_value_type_nid_postfix(values[0]) == SQLITE_TEXT) {
        sqlite3_result_error_nid_postfix(context, "probe rejects text", -1);
        return;
    }
    lastFunctionValue = sqlite3_value_double_nid_postfix(values[0]);
}

void APS5_VABI Interrupt(sqlite3_context*, int, sqlite3_value**) {
    sqlite3_interrupt_nid_postfix(interruptTarget);
}

void APS5_VABI CountStep(sqlite3_context* context, int, sqlite3_value**) {
    Require(sqlite3_user_data_nid_postfix(context) == &stepCalls, "aggregate application");
    ++stepCalls;
}

void APS5_VABI CountFinal(sqlite3_context*) {
    ++finalCalls;
}

int APS5_VABI DenyDelete(void* argument, int action, const char*, const char*, const char*, const char*) {
    Require(*static_cast<int*>(argument) == 0xA7, "authorizer argument");
    return action == SQLITE_DELETE ? SQLITE_DENY : SQLITE_OK;
}

int APS5_VABI GiveUp(void* argument, int count) {
    Require(*static_cast<int*>(argument) == 0xB5, "busy argument");
    Require(count == busyCalls, "busy count");
    ++busyCalls;
    return 0;
}

int APS5_VABI RecordWal(void* argument, sqlite3*, const char* schema, int frames) {
    Require(*static_cast<int*>(argument) == 0xC3, "wal argument");
    ++walCalls;
    walSchema = schema;
    walFrames = frames;
    return SQLITE_OK;
}

sqlite3* Open(const std::string& name) {
    sqlite3* db = nullptr;
    const int result = sqlite3_open_v2_nid_postfix(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr);
    if (result != SQLITE_OK) {
        std::fprintf(stderr, "open %s: %d %s\n", name.c_str(), result, db ? sqlite3_errmsg_nid_postfix(db) : "");
    }
    Require(result == SQLITE_OK, "open");
    Require(db != nullptr, "open handle");
    return db;
}

sqlite3_stmt* Prepare(sqlite3* db, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    const char* tail = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(db, sql, -1, &statement, &tail) == SQLITE_OK, sql);
    Require(statement && tail && *tail == '\0', "prepare tail");
    return statement;
}

void Execute(sqlite3* db, const char* sql) {
    sqlite3_stmt* statement = Prepare(db, sql);
    int result = SQLITE_ROW;
    while (result == SQLITE_ROW) {
        result = sqlite3_step_nid_postfix(statement);
    }
    Require(result == SQLITE_DONE, sql);
    Require(sqlite3_finalize_nid_postfix(statement) == SQLITE_OK, "finalize");
}

void CheckStatements(sqlite3* db) {
    Execute(db, "CREATE TABLE t(a INTEGER, b REAL, c TEXT, d BLOB)");
    sqlite3_stmt* insert = Prepare(db, "INSERT INTO t VALUES(?, ?, ?, ?)");
    Require(sqlite3_bind_parameter_count_nid_postfix(insert) == 4, "parameter count");
    Require(sqlite3_stmt_busy_nid_postfix(insert) == 0, "idle statement");
    Require(sqlite3_bind_int64_nid_postfix(insert, 1, 0x123456789AB) == SQLITE_OK, "bind int64");
    Require(sqlite3_bind_double_nid_postfix(insert, 2, 2.5) == SQLITE_OK, "bind double");
    auto* text = static_cast<char16_t*>(std::malloc(16));
    std::memcpy(text, u"hé€", 8);
    Require(sqlite3_bind_text16_nid_postfix(insert, 3, text, 6, FreeBuffer) == SQLITE_OK, "bind text16");
    Require(destroyedBuffers == 1, "text16 destructor runs after the copy");
    auto* blob = static_cast<unsigned char*>(std::malloc(8));
    std::memcpy(blob, "\x01\x02\x03\x00\x05\x06\x07\x08", 8);
    Require(sqlite3_bind_blob_nid_postfix(insert, 4, blob, 8, FreeBuffer) == SQLITE_OK, "bind blob");
    Require(destroyedBuffers == 2, "blob destructor runs after the copy");
    Require(sqlite3_step_nid_postfix(insert) == SQLITE_DONE, "insert");
    Require(sqlite3_last_insert_rowid_nid_postfix(db) == 1, "last rowid");
    Require(sqlite3_reset_nid_postfix(insert) == SQLITE_OK, "reset");
    Require(sqlite3_clear_bindings_nid_postfix(insert) == SQLITE_OK, "clear bindings");
    static const unsigned char fixed[3] = {9, 8, 7};
    Require(sqlite3_bind_int_nid_postfix(insert, 1, -7) == SQLITE_OK, "bind int");
    Require(sqlite3_bind_null_nid_postfix(insert, 2) == SQLITE_OK, "bind null");
    Require(sqlite3_bind_text16_nid_postfix(insert, 3, u"z", -1, SQLITE_TRANSIENT) == SQLITE_OK, "transient");
    Require(sqlite3_bind_blob_nid_postfix(insert, 4, fixed, 3, SQLITE_STATIC) == SQLITE_OK, "static");
    Require(destroyedBuffers == 2, "static data is not released");
    Require(sqlite3_bind_blob_nid_postfix(insert, 5, std::malloc(8), 8, FreeBuffer) != SQLITE_OK, "bad index");
    Require(destroyedBuffers == 3, "destructor runs when the bind fails");
    Require(sqlite3_step_nid_postfix(insert) == SQLITE_DONE, "second insert");
    Require(sqlite3_total_changes_nid_postfix(db) == 2, "total changes");
    Require(sqlite3_get_autocommit_nid_postfix(db) == 1, "autocommit");
    Require(sqlite3_next_stmt_nid_postfix(db, nullptr) == insert, "next statement");
    Require(sqlite3_finalize_nid_postfix(insert) == SQLITE_OK, "finalize insert");

    const char* query = "SELECT a, b, c, d FROM t ORDER BY rowid";
    sqlite3_stmt* select = Prepare(db, query);
    Require(std::strcmp(sqlite3_sql_nid_postfix(select), query) == 0, "statement text");
    Require(sqlite3_data_count_nid_postfix(select) == 0, "no row yet");
    Require(sqlite3_step_nid_postfix(select) == SQLITE_ROW, "first row");
    Require(sqlite3_stmt_busy_nid_postfix(select) != 0, "busy statement");
    Require(sqlite3_data_count_nid_postfix(select) == 4, "data count");
    Require(sqlite3_column_type_nid_postfix(select, 0) == SQLITE_INTEGER, "integer type");
    Require(sqlite3_column_int64_nid_postfix(select, 0) == 0x123456789AB, "column int64");
    Require(sqlite3_column_int_nid_postfix(select, 0) == static_cast<int>(0x456789AB), "column int");
    Require(sqlite3_column_type_nid_postfix(select, 1) == SQLITE_FLOAT, "float type");
    Require(sqlite3_column_double_nid_postfix(select, 1) == 2.5, "column double");
    Require(sqlite3_column_type_nid_postfix(select, 2) == SQLITE_TEXT, "text type");
    const auto* column = static_cast<const char16_t*>(sqlite3_column_text16_nid_postfix(select, 2));
    Require(std::memcmp(column, u"hé€", 8) == 0, "column text16");
    Require(sqlite3_column_bytes16_nid_postfix(select, 2) == 6, "column bytes16");
    Require(sqlite3_column_type_nid_postfix(select, 3) == SQLITE_BLOB, "blob type");
    Require(sqlite3_column_bytes_nid_postfix(select, 3) == 8, "column bytes");
    Require(std::memcmp(sqlite3_column_blob_nid_postfix(select, 3), "\x01\x02\x03\x00\x05\x06\x07\x08", 8) == 0,
        "column blob");
    Require(std::strcmp(sqlite3_column_decltype_nid_postfix(select, 0), "INTEGER") == 0, "decltype");
    Require(std::memcmp(sqlite3_column_name16_nid_postfix(select, 2), u"c", 4) == 0, "column name16");
    sqlite3_value* value = sqlite3_column_value_nid_postfix(select, 1);
    Require(sqlite3_value_type_nid_postfix(value) == SQLITE_FLOAT, "value type");
    Require(sqlite3_value_double_nid_postfix(value) == 2.5, "value double");
    Require(std::memcmp(sqlite3_value_text16_nid_postfix(value), u"2.5", 8) == 0, "value text16");
    Require(sqlite3_step_nid_postfix(select) == SQLITE_ROW, "second row");
    Require(sqlite3_column_int_nid_postfix(select, 0) == -7, "negative int");
    Require(sqlite3_column_type_nid_postfix(select, 1) == SQLITE_NULL, "null type");
    Require(std::memcmp(sqlite3_column_text16_nid_postfix(select, 2), u"z", 4) == 0, "transient text");
    Require(std::memcmp(sqlite3_column_blob_nid_postfix(select, 3), fixed, 3) == 0, "static blob");
    Require(sqlite3_step_nid_postfix(select) == SQLITE_DONE, "end of rows");
    Require(sqlite3_finalize_nid_postfix(select) == SQLITE_OK, "finalize select");
}

void CheckFunctions(sqlite3* db) {
    static int application = 0x42;
    Require(sqlite3_create_function_nid_postfix(db, "probe", 1, SQLITE_UTF8, &application, Probe, nullptr,
        nullptr) == SQLITE_OK, "create function");
    Execute(db, "SELECT probe(4.25)");
    Require(functionCalls == 1 && lastFunctionValue == 4.25, "function value");
    Require(lastFunctionApplication == &application, "function application");
    sqlite3_stmt* rejected = Prepare(db, "SELECT probe('x')");
    Require(sqlite3_step_nid_postfix(rejected) == SQLITE_ERROR, "function error");
    Require(std::strcmp(sqlite3_errmsg_nid_postfix(db), "probe rejects text") == 0, "function message");
    Require(sqlite3_errcode_nid_postfix(db) == SQLITE_ERROR, "function error code");
    Require(sqlite3_finalize_nid_postfix(rejected) == SQLITE_ERROR, "finalize after error");

    Require(sqlite3_create_function_nid_postfix(db, "counted", 1, SQLITE_UTF8, &stepCalls, nullptr, CountStep,
        CountFinal) == SQLITE_OK, "create aggregate");
    Execute(db, "SELECT counted(a) FROM t");
    Require(stepCalls == 2 && finalCalls == 1, "aggregate calls");
    Require(sqlite3_create_function_nid_postfix(db, "counted", 1, SQLITE_UTF8, nullptr, nullptr, nullptr,
        nullptr) == SQLITE_OK, "delete aggregate");
    sqlite3_stmt* missing = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(db, "SELECT counted(a) FROM t", -1, &missing, nullptr) == SQLITE_ERROR,
        "deleted aggregate");
    Require(missing == nullptr, "no statement for deleted aggregate");

    interruptTarget = db;
    Require(sqlite3_create_function_nid_postfix(db, "stop", 0, SQLITE_UTF8, nullptr, Interrupt, nullptr,
        nullptr) == SQLITE_OK, "create interrupt");
    sqlite3_stmt* endless = Prepare(db,
        "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) SELECT stop() FROM n");
    int result = SQLITE_ROW;
    while (result == SQLITE_ROW) {
        result = sqlite3_step_nid_postfix(endless);
    }
    Require(result == SQLITE_INTERRUPT, "interrupt");
    sqlite3_finalize_nid_postfix(endless);
}

void CheckCollation(sqlite3* db) {
    static int argument = 0x51;
    Require(sqlite3_create_collation_v2_nid_postfix(db, "reverse", SQLITE_UTF8, &argument, ReverseCompare,
        DestroyCollation) == SQLITE_OK, "create collation");
    Execute(db, "CREATE TABLE words(w TEXT)");
    Execute(db, "INSERT INTO words VALUES('alpha'), ('gamma'), ('beta')");
    sqlite3_stmt* ordered = Prepare(db, "SELECT w FROM words ORDER BY w COLLATE reverse");
    const char16_t* expected[] = {u"gamma", u"beta", u"alpha"};
    for (const char16_t* word : expected) {
        Require(sqlite3_step_nid_postfix(ordered) == SQLITE_ROW, "collation row");
        const auto* text = static_cast<const char16_t*>(sqlite3_column_text16_nid_postfix(ordered, 0));
        Require(std::u16string(text) == word, "collation order");
    }
    Require(sqlite3_finalize_nid_postfix(ordered) == SQLITE_OK, "finalize collation");
    Require(sqlite3_create_collation_v2_nid_postfix(db, "reverse", SQLITE_UTF16LE, &argument, ReverseCompare,
        DestroyCollation) == SQLITE_OK, "second encoding");
    Require(destroyedCollations == 0, "collations live until close");
}

void CheckAuthorizer(sqlite3* db) {
    static int argument = 0xA7;
    Require(sqlite3_set_authorizer_nid_postfix(db, DenyDelete, &argument) == SQLITE_OK, "set authorizer");
    sqlite3_stmt* denied = nullptr;
    Require(sqlite3_prepare_v2_nid_postfix(db, "DELETE FROM words", -1, &denied, nullptr) == SQLITE_AUTH,
        "authorizer denies");
    Require(denied == nullptr, "no denied statement");
    Require(std::strcmp(sqlite3_errstr_nid_postfix(SQLITE_AUTH), "authorization denied") == 0, "errstr");
    Require(sqlite3_set_authorizer_nid_postfix(db, nullptr, nullptr) == SQLITE_OK, "clear authorizer");
    Execute(db, "DELETE FROM words WHERE w = 'beta'");
}

void CheckExtendedCodes(sqlite3* db) {
    Execute(db, "CREATE TABLE unique_values(v INTEGER UNIQUE)");
    Execute(db, "INSERT INTO unique_values VALUES(1)");
    Require(sqlite3_extended_result_codes_nid_postfix(db, 1) == SQLITE_OK, "extended codes");
    sqlite3_stmt* duplicate = Prepare(db, "INSERT INTO unique_values VALUES(1)");
    Require(sqlite3_step_nid_postfix(duplicate) == SQLITE_CONSTRAINT_UNIQUE, "extended step code");
    Require(sqlite3_errcode_nid_postfix(db) == SQLITE_CONSTRAINT_UNIQUE, "extended errcode");
    sqlite3_finalize_nid_postfix(duplicate);
}

void CheckFileDatabase(const std::filesystem::path& directory) {
    const std::string name = "/wal.db";
    sqlite3* writer = Open(name);
    Execute(writer, "PRAGMA journal_mode = WAL");
    Execute(writer, "CREATE TABLE log(v INTEGER)");
    static int walArgument = 0xC3;
    void* previous = sqlite3_wal_hook_nid_postfix(writer, RecordWal, &walArgument);
    Require(reinterpret_cast<std::intptr_t>(previous) == 1000, "default autocheckpoint hook");
    Execute(writer, "INSERT INTO log VALUES(1)");
    Require(walCalls == 1 && walSchema == "main" && walFrames > 0, "wal hook");
    Require(sqlite3_wal_hook_nid_postfix(writer, RecordWal, &walArgument) == &walArgument, "previous wal argument");
    int logFrames = -1;
    int checkpointed = -1;
    Require(sqlite3_wal_checkpoint_v2_nid_postfix(writer, "main", SQLITE_CHECKPOINT_PASSIVE, &logFrames,
        &checkpointed) == SQLITE_OK, "checkpoint");
    Require(logFrames >= 1 && checkpointed == logFrames, "checkpoint frames");

    sqlite3* reader = Open(name);
    static int busyArgument = 0xB5;
    Require(sqlite3_busy_handler_nid_postfix(reader, GiveUp, &busyArgument) == SQLITE_OK, "busy handler");
    Execute(writer, "BEGIN EXCLUSIVE");
    Require(sqlite3_get_autocommit_nid_postfix(writer) == 0, "transaction open");
    sqlite3_stmt* blocked = Prepare(reader, "INSERT INTO log VALUES(2)");
    Require(sqlite3_step_nid_postfix(blocked) == SQLITE_BUSY, "busy");
    Require(busyCalls == 1, "busy handler called");
    sqlite3_finalize_nid_postfix(blocked);
    Require(sqlite3_busy_timeout_nid_postfix(reader, 10) == SQLITE_OK, "busy timeout");
    Execute(writer, "COMMIT");
    Execute(reader, "INSERT INTO log VALUES(2)");
    Require(busyCalls == 1, "timeout replaces the guest handler");
    Require(sqlite3_close_nid_postfix(reader) == SQLITE_OK, "close reader");
    Require(sqlite3_close_nid_postfix(writer) == SQLITE_OK, "close writer");
}

}

int main() {
    Require(sqlite3_initialize_nid_postfix() == SQLITE_OK, "initialize");
    sqlite3* db = Open(":memory:");
    CheckStatements(db);
    CheckFunctions(db);
    CheckCollation(db);
    CheckAuthorizer(db);
    CheckExtendedCodes(db);
    Require(sqlite3_close_nid_postfix(db) == SQLITE_OK, "close");
    Require(destroyedCollations == 2, "collation destructors on close");

    std::random_device random;
    const auto directory = std::filesystem::temp_directory_path() / ("nk_sqlite_" + std::to_string(random()));
    std::filesystem::create_directories(directory);
    std::filesystem::current_path(directory);
    CheckFileDatabase(directory);
    Require(std::filesystem::is_regular_file(directory / "wal.db"), "guest path maps into the root");
    std::filesystem::current_path(directory.parent_path());
    std::filesystem::remove_all(directory);
    return 0;
}
