// Arsalan Traders - Pesticide Inventory Manager - Backend Server
// Serves a JSON REST API and the phone-friendly web UI on one port.
#include "httplib.h"
#include "json.hpp"
#include "database.h"
#include <iostream>

using json = nlohmann::json;

static json pesticideToJson(const Pesticide& p) {
    return json{
        {"id", p.id}, {"name", p.name}, {"category", p.category},
        {"price", p.price}, {"quantity", p.quantity}, {"unit", p.unit},
        {"low_stock_threshold", p.low_stock_threshold},
        {"created_at", p.created_at}, {"updated_at", p.updated_at}
    };
}

static json transactionToJson(const StockTransaction& t) {
    return json{
        {"id", t.id}, {"pesticide_id", t.pesticide_id},
        {"pesticide_name", t.pesticide_name}, {"type", t.type},
        {"quantity", t.quantity}, {"note", t.note}, {"created_at", t.created_at}
    };
}

static Pesticide pesticideFromJson(const json& j) {
    Pesticide p;
    p.name = j.value("name", "");
    p.category = j.value("category", "");
    p.price = j.value("price", 0.0);
    p.quantity = j.value("quantity", 0);
    p.unit = j.value("unit", "unit");
    p.low_stock_threshold = j.value("low_stock_threshold", 10);
    return p;
}

int main() {
    Database db("arsalan_traders.db");
    if (!db.init()) {
        std::cerr << "Failed to initialize database.\n";
        return 1;
    }

    httplib::Server svr;

    // Serve the frontend (public/ folder) at the site root
    svr.set_mount_point("/", "./public");

    // Allow the frontend to call the API even during local testing
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"}
    });
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // ---- Pesticides CRUD ----
    svr.Get("/api/pesticides", [&](const httplib::Request& req, httplib::Response& res) {
        std::string search = req.has_param("search") ? req.get_param_value("search") : "";
        auto items = db.getAllPesticides(search);
        json arr = json::array();
        for (auto& p : items) arr.push_back(pesticideToJson(p));
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get(R"(/api/pesticides/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        Pesticide p;
        if (db.getPesticide(id, p)) {
            res.set_content(pesticideToJson(p).dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"error","Pesticide not found"}}.dump(), "application/json");
        }
    });

    svr.Post("/api/pesticides", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            Pesticide p = pesticideFromJson(body);
            if (p.name.empty()) {
                res.status = 400;
                res.set_content(json{{"error","Name is required"}}.dump(), "application/json");
                return;
            }
            int id = db.addPesticide(p);
            if (id < 0) {
                res.status = 500;
                res.set_content(json{{"error","Failed to add pesticide"}}.dump(), "application/json");
                return;
            }
            Pesticide created;
            db.getPesticide(id, created);
            res.status = 201;
            res.set_content(pesticideToJson(created).dump(), "application/json");
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Put(R"(/api/pesticides/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        try {
            auto body = json::parse(req.body);
            Pesticide p = pesticideFromJson(body);
            if (db.updatePesticide(id, p)) {
                Pesticide updated;
                db.getPesticide(id, updated);
                res.set_content(pesticideToJson(updated).dump(), "application/json");
            } else {
                res.status = 404;
                res.set_content(json{{"error","Pesticide not found"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/pesticides/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (db.deletePesticide(id)) {
            res.set_content(json{{"success", true}}.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"error","Pesticide not found"}}.dump(), "application/json");
        }
    });

    // ---- Stock in / out ----
    svr.Post(R"(/api/pesticides/(\d+)/stock)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        try {
            auto body = json::parse(req.body);
            std::string type = body.value("type", "");
            int qty = body.value("quantity", 0);
            std::string note = body.value("note", "");
            std::string error;
            if (db.adjustStock(id, type, qty, note, error)) {
                Pesticide updated;
                db.getPesticide(id, updated);
                res.set_content(pesticideToJson(updated).dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{{"error", error}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Get("/api/transactions", [&](const httplib::Request& req, httplib::Response& res) {
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 50;
        auto txns = db.getRecentTransactions(limit);
        json arr = json::array();
        for (auto& t : txns) arr.push_back(transactionToJson(t));
        res.set_content(arr.dump(), "application/json");
    });

    // ---- Dashboard summary ----
    svr.Get("/api/dashboard", [&](const httplib::Request&, httplib::Response& res) {
        json d;
        d["total_stock_units"] = db.totalItems();
        d["total_inventory_value"] = db.totalInventoryValue();
        auto low = db.lowStockItems();
        json lowArr = json::array();
        for (auto& p : low) lowArr.push_back(pesticideToJson(p));
        d["low_stock_items"] = lowArr;
        d["low_stock_count"] = low.size();
        res.set_content(d.dump(), "application/json");
    });

    int port = 8080;
    std::cout << "Arsalan Traders server running:\n";
    std::cout << "  On this PC:      http://localhost:" << port << "\n";
    std::cout << "  From your phone: http://<this-computer's-LAN-IP>:" << port
              << "  (phone must be on the same WiFi)\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
