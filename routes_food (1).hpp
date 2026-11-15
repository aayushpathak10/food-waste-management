// ===== PERSON 2 : Canteen + Food routes (LinkedList, MinHeap, Sorting, Binary Search) =====
#pragma once
#include "app.hpp"

inline void registerFoodRoutes(httplib::Server& svr, App& app) {
    svr.Get("/api/canteen", guarded([&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, app.canteen.toJson());
    }));

    // GET /api/foods?sort=urgency|price|name&maxPrice=50&type=RAW|PREPARED&includeExpired=0
    svr.Get("/api/foods", guarded([&](const httplib::Request& req, httplib::Response& res) {
        std::string sort = req.has_param("sort") ? req.get_param_value("sort") : "urgency";
        std::string type = req.has_param("type") ? req.get_param_value("type") : "";
        double maxPrice = -1;
        if (req.has_param("maxPrice") && !req.get_param_value("maxPrice").empty())
            maxPrice = std::stod(req.get_param_value("maxPrice"));
        bool includeExpired = !req.has_param("includeExpired") || req.get_param_value("includeExpired") != "0";

        SortInfo info;
        auto list = app.inventory.getSortedList(sort, maxPrice, type, includeExpired, info);
        json items = json::array();
        for (FoodItem* f : list) items.push_back(f->toJson());

        json meta = {{"algorithm", info.algorithm}, {"comparisons", info.comparisons},
                     {"structure", "LinkedList -> array"}, {"total", info.beforeFilter}};
        if (maxPrice >= 0) {
            meta["binarySearch"] = {{"maxPrice", maxPrice}, {"steps", info.searchSteps}, {"kept", list.size()}};
            engineLog().add("BinarySearch", "upperBound(price <= " + num(maxPrice) + ") in " +
                            std::to_string(info.searchSteps) + " steps -> " + std::to_string(list.size()) + " items");
        }
        sendJson(res, json{{"items", items}, {"meta", meta}});
    }));

    // MinHeap: top-k most urgent items (must be registered BEFORE the /api/foods/<id> route)
    svr.Get("/api/foods/urgent", guarded([&](const httplib::Request& req, httplib::Response& res) {
        int k = req.has_param("k") ? std::stoi(req.get_param_value("k")) : 3;
        json items = json::array();
        for (FoodItem* f : app.inventory.getMostUrgent(k)) items.push_back(f->toJson());
        sendJson(res, json{{"items", items}, {"heapSize", app.inventory.heapSize()}});
    }));

    // Binary search by food id
    svr.Get(R"(/api/foods/(\w+))", guarded([&](const httplib::Request& req, httplib::Response& res) {
        int steps = 0;
        FoodItem* f = app.inventory.binaryFindById(req.matches[1], steps);
        if (!f) return sendError(res, "Food item not found", 404);
        sendJson(res, json{{"item", f->toJson()}, {"binarySearchSteps", steps}});
    }));

    // Add food: RAW or PREPARED (polymorphism: RawFood / PreparedFood)
    svr.Post("/api/foods", guarded([&](const httplib::Request& req, httplib::Response& res) {
        json b = body(req);
        std::string type = strField(b, "type", "RAW");
        std::string id = app.inventory.generateId();
        time_t now = nowTs();
        FoodItem* f = nullptr;
        if (type == "RAW") {
            f = new RawFood(id, strField(b, "name"), numField(b, "quantity"), strField(b, "unit", "kg"),
                            numField(b, "price"), app.canteen.getId(), now, strField(b, "storageType", "Room"),
                            static_cast<int>(numField(b, "shelfLifeDays", 0)));
        } else if (type == "PREPARED") {
            double ago = numField(b, "cookedHoursAgo", 0);
            if (ago < 0) throw std::invalid_argument("Cooked hours ago cannot be negative");
            f = new PreparedFood(id, strField(b, "name"), numField(b, "quantity"), strField(b, "unit", "kg"),
                                 numField(b, "price"), app.canteen.getId(), now,
                                 now - static_cast<time_t>(ago * 3600), numField(b, "consumeWithinHours", 0));
        } else {
            throw std::invalid_argument("Type must be RAW or PREPARED");
        }
        app.canteen.addFoodItem(app.inventory, f);
        app.prediction.recordDailyWaste(f->getType(), fmtDate(now), f->getQuantity());
        app.saveAll();
        sendJson(res, json{{"item", f->toJson()}}, 201);
    }));

    svr.Delete(R"(/api/foods/(\w+))", guarded([&](const httplib::Request& req, httplib::Response& res) {
        if (!app.canteen.removeFoodItem(app.inventory, req.matches[1])) return sendError(res, "Food item not found", 404);
        app.saveAll();
        sendJson(res, json{{"removed", std::string(req.matches[1])}});
    }));
}
