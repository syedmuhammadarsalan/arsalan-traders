#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>

struct Pesticide {
    int id = 0;
    std::string name;
    std::string category;
    double price = 0.0;
    int quantity = 0;
    std::string unit;
    int low_stock_threshold = 10;
    std::string created_at;
    std::string updated_at;
};

struct StockTransaction {
    int id = 0;
    int pesticide_id = 0;
    std::string pesticide_name;
    std::string type;      // "IN" or "OUT"
    int quantity = 0;
    std::string note;
    std::string created_at;
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    bool init();

    // Pesticides
    std::vector<Pesticide> getAllPesticides(const std::string& search = "");
    bool getPesticide(int id, Pesticide& out);
    int  addPesticide(const Pesticide& p);
    bool updatePesticide(int id, const Pesticide& p);
    bool deletePesticide(int id);

    // Stock movement
    bool adjustStock(int pesticide_id, const std::string& type, int qty,
                      const std::string& note, std::string& errorOut);
    std::vector<StockTransaction> getRecentTransactions(int limit = 50);

    // Dashboard
    int totalItems();
    double totalInventoryValue();
    std::vector<Pesticide> lowStockItems();

private:
    sqlite3* db_ = nullptr;
};
