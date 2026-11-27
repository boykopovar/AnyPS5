#include "prx/libSceNKWebKitRequirements/include/Sqlite.hpp"
#include "tests/AppArchiveFixture.hpp"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <iterator>

namespace {

void Require(bool ok, const char* message) {
    if (ok) return;
    std::fprintf(stderr, "SQLite archive test failed: %s\n", message);
    std::abort();
}

int Run(sqlite3* database, const char* sql, int* value = nullptr) {
    sqlite3_stmt* statement = nullptr;
    int result = sqlite3_prepare_v2_nid_postfix(database, sql, -1, &statement, nullptr);
    if (result == SQLITE_OK) {
        result = sqlite3_step_nid_postfix(statement);
        if (value && result == SQLITE_ROW) *value = sqlite3_column_int_nid_postfix(statement, 0);
    }
    sqlite3_finalize_nid_postfix(statement);
    return result;
}

std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "open fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}

void CheckValue(sqlite3* database, const char* query, int expected) {
    int actual = 0;
    Require(Run(database, query, &actual) == SQLITE_ROW && actual == expected, query);
}

}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = AppArchiveFixture::EnterFreshRoot("nk-sqlite-archive");
    sqlite3* seed = nullptr;
    Require(sqlite3_open("seed.db", &seed) == SQLITE_OK, "create reference database");
    Require(sqlite3_exec(seed, "CREATE TABLE t(v, payload); INSERT INTO t VALUES(42, CAST(zeroblob(160000) || x'4D' AS BLOB))",
        nullptr, nullptr, nullptr) == SQLITE_OK, "write reference data");
    auto snapshot = Read("seed.db");
    Require(sqlite3_exec(seed, "PRAGMA journal_mode=WAL; UPDATE t SET v=71", nullptr, nullptr, nullptr) == SQLITE_OK,
        "create a committed WAL frame");
    const auto wal = Read("seed.db-wal");
    Require(!wal.empty(), "fixture has pending WAL content");
    AppArchiveFixture::WriteArchive(root / "app0.zar", {
        {"data/Snapshot.db", snapshot}, {"shadow.db", snapshot}, {"wal.db", Read("seed.db")}, {"wal.db-wal", wal},
        {"journal.db", snapshot}, {"journal.db-journal", {1}}, {"loose-wal.db", snapshot}
    });
    Require(sqlite3_close(seed) == SQLITE_OK, "checkpoint and close reference database");
    std::filesystem::copy_file("seed.db", root / "app0" / "shadow.db");
    std::ofstream(root / "app0" / "loose-wal.db-wal", std::ios::binary).put('w');

    sqlite3* database = nullptr;
    for (int flags : {SQLITE_OPEN_READONLY, SQLITE_OPEN_READWRITE, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE}) {
        Require(sqlite3_open_v2_nid_postfix("/app0/data/Snapshot.db", &database, flags, nullptr) == SQLITE_OK,
            "open archive-only database");
        CheckValue(database, "SELECT v FROM t", 42);
        CheckValue(database, "SELECT hex(substr(payload,160000,2)) = '004D' FROM t", 1);
        Require(Run(database, "UPDATE t SET v=99") == SQLITE_READONLY, "archived database rejects writes");
        Require(sqlite3_close_nid_postfix(database) == SQLITE_OK, "close archived database");
    }

    Require(sqlite3_open_v2_nid_postfix("file:/app0/data/Snapshot.db?mode=ro", &database,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr) == SQLITE_OK, "open archived database URI");
    CheckValue(database, "SELECT v FROM t", 42);
    Require(sqlite3_close_nid_postfix(database) == SQLITE_OK, "close archived database URI");

    Require(sqlite3_open_v2_nid_postfix(":memory:", &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
        "open memory database");
    Require(Run(database, "ATTACH '/app0/data/Snapshot.db' AS packed") == SQLITE_DONE, "attach archived database");
    CheckValue(database, "SELECT v FROM packed.t", 42);
    Require(Run(database, "INSERT INTO packed.t VALUES(10,NULL)") == SQLITE_READONLY, "reject attached write");
    Require(Run(database, "DETACH packed") == SQLITE_DONE, "detach archived database");
    Require(sqlite3_close_nid_postfix(database) == SQLITE_OK, "close memory database");

    Require(sqlite3_open_v2_nid_postfix("/app0/shadow.db", &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK,
        "open loose database");
    CheckValue(database, "SELECT v FROM t", 71);
    Require(Run(database, "UPDATE t SET v=74") == SQLITE_DONE, "loose database remains writable");
    Require(sqlite3_close_nid_postfix(database) == SQLITE_OK, "close loose database");

    for (const char* path : {"/app0/data", "/app0/missing.db", "/app0/wal.db", "/app0/journal.db", "/app0/loose-wal.db"}) {
        Require(sqlite3_open_v2_nid_postfix(path, &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_CANTOPEN, path);
        Require(sqlite3_close_nid_postfix(database) == SQLITE_OK, "close refused handle");
    }
    Require(std::distance(std::filesystem::directory_iterator(root / "app0"), {}) == 3,
        "only the loose database, existing sidecar and sce_sys remain in app0");
    Require(!std::filesystem::exists(root / "app0" / "data"), "archived database stays packed");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
