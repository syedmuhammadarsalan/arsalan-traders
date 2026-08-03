#include "database.h"
#include <iostream>
#include <stdexcept>

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }
    // Better concurrency for a small multi-request web server
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

bool Database::init() {
    const char* sql = R"SQL(
    CREATE TABLE IF NOT EXISTS pesticides (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        category TEXT DEFAULT '',
        price REAL NOT NULL DEFAULT 0,
        quantity INTEGER NOT NULL DEFAULT 0,
        unit TEXT DEFAULT 'unit',
        low_stock_threshold INTEGER NOT NULL DEFAULT 10,
        created_at TEXT DEFAULT (datetime('now')),
        updated_at TEXT DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS stock_transactions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        pesticide_id INTEGER NOT NULL,
        type TEXT NOT NULL CHECK(type IN ('IN','OUT')),
        quantity INTEGER NOT NULL,
        note TEXT DEFAULT '',
        created_at TEXT DEFAULT (datetime('now')),
        FOREIGN KEY(pesticide_id) REFERENCES pesticides(id) ON DELETE CASCADE
    );

    CREATE INDEX IF NOT EXISTS idx_pesticides_name ON pesticides(name);
    CREATE INDEX IF NOT EXISTS idx_transactions_pesticide ON stock_transactions(pesticide_id);
    )SQL";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Init error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

static Pesticide rowToPesticide(sqlite3_stmt* stmt) {
    Pesticide p;
    p.id = sqlite3_column_int(stmt, 0);
    p.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const unsigned char* cat = sqlite3_column_text(stmt, 2);
    p.category = cat ? reinterpret_cast<const char*>(cat) : "";
    p.price = sqlite3_column_double(stmt, 3);
    p.quantity = sqlite3_column_int(stmt, 4);
    const unsigned char* unit = sqlite3_column_text(stmt, 5);
    p.unit = unit ? reinterpret_cast<const char*>(unit) : "unit";
    p.low_stock_threshold = sqlite3_column_int(stmt, 6);
    const unsigned char* ca = sqlite3_column_text(stmt, 7);
    p.created_at = ca ? reinterpret_cast<const char*>(ca) : "";
    const unsigned char* ua = sqlite3_column_text(stmt, 8);
    p.updated_at = ua ? reinterpret_cast<const char*>(ua) : "";
    return p;
}

std::vector<Pesticide> Database::getAllPesticides(const std::string& search) {
    std::vector<Pesticide> results;
    std::string sql =
        "SELECT id,name,category,price,quantity,unit,low_stock_threshold,created_at,updated_at "
        "FROM pesticides";
    if (!search.empty()) sql += " WHERE name LIKE ? OR category LIKE ?";
    sql += " ORDER BY name COLLATE NOCASE ASC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

    if (!search.empty()) {
        std::string like = "%" + search + "%";
        sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, like.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToPesticide(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool Database::getPesticide(int id, Pesticide& out) {
    const char* sql =
        "SELECT id,name,category,price,quantity,unit,low_stock_threshold,created_at,updated_at "
        "FROM pesticides WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out = rowToPesticide(stmt);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

int Database::addPesticide(const Pesticide& p) {
    const char* sql =
        "INSERT INTO pesticides (name,category,price,quantity,unit,low_stock_threshold) "
        "VALUES (?,?,?,?,?,?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, p.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, p.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, p.price);
    sqlite3_bind_int(stmt, 4, p.quantity);
    sqlite3_bind_text(stmt, 5, p.unit.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, p.low_stock_threshold);

    int newId = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        newId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    }
    sqlite3_finalize(stmt);
    return newId;
}

bool Database::updatePesticide(int id, const Pesticide& p) {
    const char* sql =
        "UPDATE pesticides SET name=?, category=?, price=?, quantity=?, unit=?, "
        "low_stock_threshold=?, updated_at=datetime('now') WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, p.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, p.category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, p.price);
    sqlite3_bind_int(stmt, 4, p.quantity);
    sqlite3_bind_text(stmt, 5, p.unit.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, p.low_stock_threshold);
    sqlite3_bind_int(stmt, 7, id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db_) > 0;
}

bool Database::deletePesticide(int id) {
    const char* sql = "DELETE FROM pesticides WHERE id=?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db_) > 0;
}

bool Database::adjustStock(int pesticide_id, const std::string& type, int qty,
                            const std::string& note, std::string& errorOut) {
    if (qty <= 0) { errorOut = "Quantity must be positive"; return false; }
    if (type != "IN" && type != "OUT") { errorOut = "Type must be IN or OUT"; return false; }

    Pesticide p;
    if (!getPesticide(pesticide_id, p)) { errorOut = "Pesticide not found"; return false; }

    if (type == "OUT" && qty > p.quantity) {
        errorOut = "Not enough stock (have " + std::to_string(p.quantity) + ")";
        return false;
    }

    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

    int newQty = (type == "IN") ? p.quantity + qty : p.quantity - qty;
    const char* updSql = "UPDATE pesticides SET quantity=?, updated_at=datetime('now') WHERE id=?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, updSql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, newQty);
    sqlite3_bind_int(stmt, 2, pesticide_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* insSql =
        "INSERT INTO stock_transactions (pesticide_id,type,quantity,note) VALUES (?,?,?,?);";
    sqlite3_prepare_v2(db_, insSql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, pesticide_id);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, qty);
    sqlite3_bind_text(stmt, 4, note.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

std::vector<StockTransaction> Database::getRecentTransactions(int limit) {
    std::vector<StockTransaction> results;
    const char* sql =
        "SELECT t.id, t.pesticide_id, p.name, t.type, t.quantity, t.note, t.created_at "
        "FROM stock_transactions t JOIN pesticides p ON p.id = t.pesticide_id "
        "ORDER BY t.id DESC LIMIT ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StockTransaction t;
        t.id = sqlite3_column_int(stmt, 0);
        t.pesticide_id = sqlite3_column_int(stmt, 1);
        t.pesticide_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        t.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        t.quantity = sqlite3_column_int(stmt, 4);
        const unsigned char* note = sqlite3_column_text(stmt, 5);
        t.note = note ? reinterpret_cast<const char*>(note) : "";
        t.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        results.push_back(t);
    }
    sqlite3_finalize(stmt);
    return results;
}

int Database::totalItems() {
    const char* sql = "SELECT COALESCE(SUM(quantity),0) FROM pesticides;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

double Database::totalInventoryValue() {
    const char* sql = "SELECT COALESCE(SUM(price*quantity),0) FROM pesticides;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    double total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

std::vector<Pesticide> Database::lowStockItems() {
    std::vector<Pesticide> results;
    const char* sql =
        "SELECT id,name,category,price,quantity,unit,low_stock_threshold,created_at,updated_at "
        "FROM pesticides WHERE quantity <= low_stock_threshold ORDER BY quantity ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToPesticide(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}
