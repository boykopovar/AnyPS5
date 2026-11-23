#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>

using NKWebKit::WebCoreAbi::CollationCallable;
using NKWebKit::WebCoreAbi::CollationFunction;
using NKWebKit::WebCoreAbi::SQLiteDatabase;
using NKWebKit::WebCoreAbi::SQLiteStatement;
using NKWebKit::WebCoreAbi::SQLiteStatementAutoResetScope;
using NKWebKit::WebCoreAbi::SQLiteTransaction;
using NKWebKit::WebCoreAbi::SQLValue;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::Vector;

static_assert(std::is_same_v<decltype(&_ZN7WebCore14SQLiteDatabase4openERKN3WTF6StringENS0_8OpenModeE),
    bool (APS5_VABI*)(SQLiteDatabase*, const String*, int)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore15SQLiteStatement13getColumnTextEi), String (APS5_VABI*)(SQLiteStatement*, int)>);
static_assert(std::is_same_v<decltype(&_ZNK7WebCore15SQLiteStatement18bindParameterCountEv),
    unsigned (APS5_VABI*)(const SQLiteStatement*)>);

namespace {

constexpr int SqliteOk = 0;
constexpr int SqliteError = 1;
constexpr int SqliteCantOpen = 14;
constexpr int SqliteMisuse = 21;
constexpr int SqliteRow = 100;
constexpr int SqliteDone = 101;
constexpr int ReadOnly = 0;
constexpr int ReadWriteCreate = 2;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore SQLite check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

StringImpl Latin1(const char* text) {
    return StringImpl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
}

StringImpl Utf16(std::u16string_view text) {
    return StringImpl{4, static_cast<unsigned>(text.size()), text.data(), 0};
}

std::u16string Units(const String& string) {
    std::u16string units;
    for (unsigned i = 0; string.impl && i < string.impl->length; ++i) {
        units.push_back((string.impl->hashAndFlags & 4) ? static_cast<const unsigned char*>(string.impl->data)[i]
                                                        : static_cast<const char16_t*>(string.impl->data)[i]);
    }
    return units;
}

bool Equals(const String& string, std::u16string_view expected) {
    return string.impl && Units(string) == expected;
}

struct Database {
    alignas(SQLiteDatabase) unsigned char storage[sizeof(SQLiteDatabase)];
    SQLiteDatabase* operator->() { return reinterpret_cast<SQLiteDatabase*>(storage); }
    SQLiteDatabase* get() { return reinterpret_cast<SQLiteDatabase*>(storage); }
    Database() {
        std::memset(storage, 0xA5, sizeof(storage));
        _ZN7WebCore14SQLiteDatabaseC1Ev(get());
    }
    ~Database() { _ZN7WebCore14SQLiteDatabaseD1Ev(get()); }
    bool Open(const char* path, int mode = ReadWriteCreate) {
        auto name = Latin1(path);
        String filename(&name);
        return _ZN7WebCore14SQLiteDatabase4openERKN3WTF6StringENS0_8OpenModeE(get(), &filename, mode);
    }
    bool Execute(const char* sql) {
        auto text = Latin1(sql);
        String command(&text);
        return _ZN7WebCore14SQLiteDatabase14executeCommandERKN3WTF6StringE(get(), &command);
    }
    bool TableExists(const char* table) {
        auto text = Latin1(table);
        String name(&text);
        return _ZN7WebCore14SQLiteDatabase11tableExistsERKN3WTF6StringE(get(), &name);
    }
};

struct Statement {
    StringImpl text;
    String sql;
    alignas(SQLiteStatement) unsigned char storage[sizeof(SQLiteStatement)];
    SQLiteStatement* get() { return reinterpret_cast<SQLiteStatement*>(storage); }
    Statement(Database& db, const char* query) : text(Latin1(query)), sql(&text) {
        _ZN7WebCore15SQLiteStatementC1ERNS_14SQLiteDatabaseERKN3WTF6StringE(get(), db.get(), &sql);
    }
    ~Statement() { _ZN7WebCore15SQLiteStatementD1Ev(get()); }
    int Prepare() { return _ZN7WebCore15SQLiteStatement7prepareEv(get()); }
    int Step() { return _ZN7WebCore15SQLiteStatement4stepEv(get()); }
    int Int(int col) { return _ZN7WebCore15SQLiteStatement12getColumnIntEi(get(), col); }
};

void CheckClosedDatabase() {
    Database db;
    Require(db->db == nullptr && db->pageSize == -1 && db->openError == SqliteError);
    Require(db->openErrorMessage.buffer == nullptr && db->lastChangesCount == 0 && !db->useWAL);
    Require(_ZN7WebCore14SQLiteDatabase9lastErrorEv(db.get()) == SqliteError);
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "database is not open") == 0);
    Require(!db.TableExists("anything"));
    Require(!db.Execute("CREATE TABLE t(v)"));
    _ZN7WebCore14SQLiteDatabase9interruptEv(db.get());
    Statement statement(db, "SELECT 1");
    Require(statement.Prepare() == SqliteMisuse);
    Require(_ZN7WebCore15SQLiteStatement8bindBlobEiPKvi(statement.get(), 1, "x", 1) == SqliteError);
    Require(_ZN7WebCore15SQLiteStatement7bindIntEii(statement.get(), 1, 1) == SqliteMisuse);
    Require(_ZNK7WebCore15SQLiteStatement18bindParameterCountEv(statement.get()) == 0);
}

void CheckOpenFailures(const std::filesystem::path& directory) {
    Database db;
    Require(!db.Open("/webcore-sqlite/missing.db", ReadOnly));
    Require(db->db == nullptr && _ZN7WebCore14SQLiteDatabase9lastErrorEv(db.get()) == SqliteCantOpen);
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "unable to open database file") == 0);
    Require(!std::filesystem::exists(directory / "missing.db"));
    _ZN7WebCore14SQLiteDatabase5closeEv(db.get());
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "database is not open") == 0);

    _ZN7WebCore14SQLiteDatabase29setIsDatabaseOpeningForbiddenEb(true);
    Require(!db.Open("/webcore-sqlite/forbidden.db"));
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "opening database is forbidden") == 0);
    Require(_ZN7WebCore14SQLiteDatabase9lastErrorEv(db.get()) == SqliteError);
    _ZN7WebCore14SQLiteDatabase29setIsDatabaseOpeningForbiddenEb(false);
    Require(!std::filesystem::exists(directory / "forbidden.db"));
}

void CheckOpen(Database& db, const std::filesystem::path& directory) {
    Require(db.Open("/webcore-sqlite/main.db"));
    Require(db->db != nullptr && db->useWAL);
    Require(std::filesystem::is_regular_file(directory / "main.db"));
    Require(std::filesystem::is_regular_file(directory / "main.db-wal"));
    Statement journal(db, "PRAGMA journal_mode");
    Require(Equals(_ZN7WebCore15SQLiteStatement13getColumnTextEi(journal.get(), 0), u"wal"));
    Statement tempStore(db, "  PRAGMA temp_store;\n\t");
    Require(tempStore.Int(0) == 2);
    Statement twoStatements(db, "SELECT 1; SELECT 2");
    Require(twoStatements.Prepare() == SqliteError);
    Statement printfCall(db, "SELECT printf('%d', 1)");
    Require(printfCall.Prepare() == SqliteOk && printfCall.Step() == SqliteError);
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "Function printf is unauthorized") == 0);
    Require(!db.Execute("INSERT INTO missing VALUES(1)"));
    Require(_ZN7WebCore14SQLiteDatabase9lastErrorEv(db.get()) == SqliteError);
    Require(std::strcmp(_ZN7WebCore14SQLiteDatabase12lastErrorMsgEv(db.get()), "no such table: missing") == 0);
    Require(!db.Execute(""));
}

void CheckBindAndRead(Database& db) {
    Require(db.Execute("CREATE TABLE items(i INTEGER, big INTEGER, d REAL, t TEXT, b BLOB, n)"));
    Require(db.TableExists("items") && !db.TableExists("item") && !db.TableExists("ITEMS'"));
    Require(!db.TableExists("it'em") && db.Execute("CREATE TABLE \"it'em\"(v)") && db.TableExists("it'em"));
    Require(!db.TableExists("it''em") && db.Execute("DROP TABLE \"it'em\"") && !db.TableExists("it'em"));
    Require(!db.TableExists("x'; DROP TABLE items; --") && db.TableExists("items"));
    Require(!db.TableExists("x' OR '1'='1") && !db.TableExists("") && !db.TableExists("%"));
    Require(db.Execute("CREATE TABLE \"x'; DROP TABLE items; --\"(v)") && db.TableExists("x'; DROP TABLE items; --"));
    Require(db.Execute("DROP TABLE \"x'; DROP TABLE items; --\"") && db.TableExists("items"));
    Statement insert(db, "INSERT INTO items VALUES(?, ?, ?, ?, ?, ?)");
    Require(insert.Prepare() == SqliteOk);
    Require(_ZNK7WebCore15SQLiteStatement18bindParameterCountEv(insert.get()) == 6);
    auto unicode = Utf16(u"café 中");
    String unicodeText(&unicode);
    Require(_ZN7WebCore15SQLiteStatement7bindIntEii(insert.get(), 1, -7) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement9bindInt64Eil(insert.get(), 2, 0x123456789ALL) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement10bindDoubleEid(insert.get(), 3, 2.5) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement8bindTextEiRKN3WTF6StringE(insert.get(), 4, &unicodeText) == SqliteOk);
    const unsigned char bytes[] = {0, 1, 0xFE, 0x80};
    Require(_ZN7WebCore15SQLiteStatement8bindBlobEiPKvi(insert.get(), 5, bytes, sizeof(bytes)) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement8bindNullEi(insert.get(), 6) == SqliteOk);
    Require(insert.Step() == SqliteDone);
    Require(_ZN7WebCore15SQLiteStatement5resetEv(insert.get()) == SqliteOk);

    auto latin = Latin1("plain");
    String latinText(&latin);
    String nullText;
    SQLValue nullValue{};
    nullValue.index = 0;
    SQLValue stringValue{};
    stringValue.string = &latin;
    stringValue.index = 1;
    SQLValue doubleValue{};
    doubleValue.number = -0.25;
    doubleValue.index = 2;
    Require(_ZN7WebCore15SQLiteStatement9bindValueEiRKN3WTF7VariantIJDnNS1_6StringEdEEE(insert.get(), 1, &nullValue) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement9bindValueEiRKN3WTF7VariantIJDnNS1_6StringEdEEE(insert.get(), 3, &doubleValue) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement9bindValueEiRKN3WTF7VariantIJDnNS1_6StringEdEEE(insert.get(), 4, &stringValue) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement8bindBlobEiRKN3WTF6StringE(insert.get(), 5, &latinText) == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement8bindTextEiRKN3WTF6StringE(insert.get(), 6, &nullText) == SqliteOk);
    Require(insert.Step() == SqliteDone);
    Require(latin.refCount == 4 && unicode.refCount == 4);

    Statement select(db, "SELECT i, big, d, t, b, n, typeof(n), 1 + 1 FROM items ORDER BY rowid");
    Require(select.Prepare() == SqliteOk && select.Step() == SqliteRow);
    auto* query = select.get();
    Require(_ZN7WebCore15SQLiteStatement12getColumnIntEi(query, 0) == -7);
    Require(_ZN7WebCore15SQLiteStatement14getColumnInt64Ei(query, 1) == 0x123456789ALL);
    Require(_ZN7WebCore15SQLiteStatement15getColumnDoubleEi(query, 2) == 2.5);
    Require(Equals(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 3), u"café 中"));
    Require(_ZN7WebCore15SQLiteStatement12isColumnNullEi(query, 5) && !_ZN7WebCore15SQLiteStatement12isColumnNullEi(query, 0));
    Require(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 5).impl == nullptr);
    Require(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 9).impl == nullptr);
    Require(_ZN7WebCore15SQLiteStatement12getColumnIntEi(query, 9) == 0 && !_ZN7WebCore15SQLiteStatement12isColumnNullEi(query, 9));
    Require(_ZN7WebCore15SQLiteStatement22isColumnDeclaredAsBlobEi(query, 4));
    Require(!_ZN7WebCore15SQLiteStatement22isColumnDeclaredAsBlobEi(query, 3));
    Vector<char> chars;
    _ZN7WebCore15SQLiteStatement21getColumnBlobAsVectorEiRN3WTF6VectorIcLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(query, 4, &chars);
    Require(chars.size == 4 && chars.capacity == 16 && std::memcmp(chars.buffer, bytes, 4) == 0);
    Vector<unsigned char> blob(20);
    blob.size = 20;
    auto* reused = blob.buffer;
    _ZN7WebCore15SQLiteStatement21getColumnBlobAsVectorEiRN3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(query, 4, &blob);
    Require(blob.size == 4 && blob.capacity == 20 && blob.buffer == reused && blob[2] == 0xFE);
    _ZN7WebCore15SQLiteStatement21getColumnBlobAsVectorEiRN3WTF6VectorIhLm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(query, 5, &blob);
    Require(blob.size == 0 && blob.capacity == 0 && blob.buffer == nullptr);
    Require(_ZN7WebCore15SQLiteStatement21getColumnBlobAsStringEi(query, 5).impl == _ZN7WebCore15SQLiteStatement21getColumnBlobAsStringEi(query, 5).impl);
    Require(Equals(_ZN7WebCore15SQLiteStatement21getColumnBlobAsStringEi(query, 5), u""));

    Require(select.Step() == SqliteRow);
    Require(_ZN7WebCore15SQLiteStatement12isColumnNullEi(query, 0));
    Require(_ZN7WebCore15SQLiteStatement15getColumnDoubleEi(query, 2) == -0.25);
    Require(Equals(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 3), u"plain"));
    Require(Equals(_ZN7WebCore15SQLiteStatement21getColumnBlobAsStringEi(query, 4), u"plain"));
    Require(Equals(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 6), u"text"));
    Require(Equals(_ZN7WebCore15SQLiteStatement13getColumnTextEi(query, 5), u""));
    Require(select.Step() == SqliteDone);
    Require(_ZN7WebCore15SQLiteStatement8finalizeEv(query) == SqliteOk && query->statement == nullptr);
    Require(select.Step() == SqliteOk);

    Statement count(db, "SELECT count(*) FROM items");
    Require(count.Int(0) == 2 && count.get()->statement != nullptr);
}

void CheckTransactions(Database& db) {
    alignas(SQLiteTransaction) unsigned char storage[sizeof(SQLiteTransaction)];
    auto* transaction = reinterpret_cast<SQLiteTransaction*>(storage);
    auto rows = [&] {
        Statement count(db, "SELECT count(*) FROM items");
        return count.Int(0);
    };
    _ZN7WebCore17SQLiteTransactionC1ERNS_14SQLiteDatabaseEb(transaction, db.get(), false);
    Require(transaction->db == db.get() && !transaction->inProgress && !transaction->readOnly);
    _ZN7WebCore17SQLiteTransaction5beginEv(transaction);
    Require(transaction->inProgress && db->transactionInProgress);
    Require(db.Execute("INSERT INTO items(i) VALUES(10)"));
    _ZN7WebCore17SQLiteTransaction8rollbackEv(transaction);
    Require(!transaction->inProgress && !db->transactionInProgress && rows() == 2);
    _ZN7WebCore17SQLiteTransaction5beginEv(transaction);
    Require(db.Execute("INSERT INTO items(i) VALUES(11)"));
    _ZN7WebCore17SQLiteTransaction6commitEv(transaction);
    Require(!transaction->inProgress && !db->transactionInProgress && rows() == 3);
    _ZN7WebCore17SQLiteTransaction5beginEv(transaction);
    Require(db.Execute("INSERT INTO items(i) VALUES(12)"));
    _ZN7WebCore17SQLiteTransactionD1Ev(transaction);
    Require(!db->transactionInProgress && rows() == 3);

    _ZN7WebCore17SQLiteTransactionC2ERNS_14SQLiteDatabaseEb(transaction, db.get(), true);
    _ZN7WebCore17SQLiteTransaction5beginEv(transaction);
    Require(transaction->readOnly && transaction->inProgress);
    Require(!db.Execute("BEGIN"));
    _ZN7WebCore17SQLiteTransaction6commitEv(transaction);
    Require(!transaction->inProgress);
    _ZN7WebCore17SQLiteTransaction6commitEv(transaction);
    _ZN7WebCore17SQLiteTransactionD2Ev(transaction);
}

void CheckAutoResetScope(Database& db) {
    Statement select(db, "SELECT i FROM items WHERE i = ?");
    Require(select.Prepare() == SqliteOk);
    Require(_ZN7WebCore15SQLiteStatement7bindIntEii(select.get(), 1, 11) == SqliteOk && select.Step() == SqliteRow);
    SQLiteStatementAutoResetScope first{nullptr};
    _ZN7WebCore29SQLiteStatementAutoResetScopeC1EPNS_15SQLiteStatementE(&first, reinterpret_cast<std::uintptr_t>(select.get()));
    SQLiteStatementAutoResetScope second{nullptr};
    _ZN7WebCore29SQLiteStatementAutoResetScopeC1EOS0_(&second, &first);
    Require(first.statement == nullptr && second.statement == select.get());
    SQLiteStatementAutoResetScope third{nullptr};
    Require(&_ZN7WebCore29SQLiteStatementAutoResetScopeaSEOS0_(&third, &second) == &third);
    Require(third.statement == select.get() && second.statement == nullptr);
    _ZN7WebCore29SQLiteStatementAutoResetScopeD1Ev(&first);
    _ZN7WebCore29SQLiteStatementAutoResetScopeD2Ev(&second);
    _ZN7WebCore29SQLiteStatementAutoResetScopeD1Ev(&third);
    Require(select.Step() == SqliteDone);
    SQLiteStatementAutoResetScope moved{nullptr};
    _ZN7WebCore29SQLiteStatementAutoResetScopeC2EOS0_(&moved, &third);
    Require(moved.statement == select.get());
}

int compareCalls = 0;
int destroyedCallables = 0;

void APS5_VABI UnexpectedDestructor(CollationCallable*) {
    std::abort();
}

void APS5_VABI DeletingDestructor(CollationCallable*) {
    ++destroyedCallables;
}

int APS5_VABI ReverseCompare(CollationCallable*, int aLength, const void* a, int bLength, const void* b) {
    ++compareCalls;
    const int order = std::memcmp(a, b, static_cast<std::size_t>(aLength < bLength ? aLength : bLength));
    return order ? -order : bLength - aLength;
}

void* const collationVtable[] = {
    reinterpret_cast<void*>(&UnexpectedDestructor),
    reinterpret_cast<void*>(&DeletingDestructor),
    reinterpret_cast<void*>(&ReverseCompare),
};

void CheckDatabaseCommands(Database& db) {
    CollationCallable callable{collationVtable};
    CollationFunction function{&callable};
    auto name = Latin1("REVERSE");
    String collation(&name);
    _ZN7WebCore14SQLiteDatabase20setCollationFunctionERKN3WTF6StringEONS1_8FunctionIFiiPKviS7_EEE(db.get(), &collation, &function);
    Require(function.callableWrapper == nullptr);
    Require(db.Execute("CREATE TABLE words(w TEXT)") && db.Execute("INSERT INTO words VALUES('a'), ('c'), ('b')"));
    {
        Statement ordered(db, "SELECT w FROM words ORDER BY w COLLATE REVERSE");
        std::u16string order;
        Require(ordered.Prepare() == SqliteOk);
        while (ordered.Step() == SqliteRow) order += Units(_ZN7WebCore15SQLiteStatement13getColumnTextEi(ordered.get(), 0));
        Require(order == u"cba" && compareCalls > 0);
    }
    Require(_ZN7WebCore14SQLiteDatabase16runVacuumCommandEv(db.get()) == SqliteOk);
    Require(_ZN7WebCore14SQLiteDatabase27turnOnIncrementalAutoVacuumEv(db.get()));
    {
        Statement autoVacuum(db, "PRAGMA auto_vacuum");
        Require(autoVacuum.Int(0) == 2);
    }
    Require(_ZN7WebCore14SQLiteDatabase27turnOnIncrementalAutoVacuumEv(db.get()));
    Require(_ZN7WebCore14SQLiteDatabase27runIncrementalVacuumCommandEv(db.get()) == SqliteOk);
    _ZN7WebCore14SQLiteDatabase9interruptEv(db.get());
    Require(db.Execute("CREATE TABLE counted(id INTEGER PRIMARY KEY AUTOINCREMENT, v)"));
    Require(db.Execute("INSERT INTO counted(v) VALUES(1)") && db.TableExists("sqlite_sequence"));
    _ZN7WebCore14SQLiteDatabase14clearAllTablesEv(db.get());
    Require(!db.TableExists("items") && !db.TableExists("words") && !db.TableExists("counted"));
    Require(db.TableExists("sqlite_sequence"));
    Require(destroyedCallables == 0);
    _ZN7WebCore14SQLiteDatabase5closeEv(db.get());
    Require(destroyedCallables == 1 && db->db == nullptr && db->openError == SqliteError);
    Require(!db.TableExists("sqlite_sequence"));
}

}  // namespace

int main() {
    std::random_device random;
    const auto directory = std::filesystem::temp_directory_path() / ("nkwebkit_sqlite_" + std::to_string(random()));
    std::filesystem::create_directories(directory);
    AddPathAlias_nid_no_patch("/webcore-sqlite", directory.string().c_str());
    CheckClosedDatabase();
    CheckOpenFailures(directory);
    {
        Database db;
        CheckOpen(db, directory);
        CheckBindAndRead(db);
        CheckTransactions(db);
        CheckAutoResetScope(db);
        CheckDatabaseCommands(db);
        Require(db.Open("/webcore-sqlite/main.db", ReadOnly) && db->db != nullptr);
    }
    std::filesystem::remove_all(directory);
    return 0;
}
