// ===== PERSON 4 : Prediction, Nearest NGO (Dijkstra), Stats, Engine log, Console demo =====
#pragma once
#include "app.hpp"

inline void registerInsightRoutes(httplib::Server& svr, App& app) {
    // Moving-average prediction of tomorrow's waste
    svr.Get("/api/prediction", guarded([&](const httplib::Request& req, httplib::Response& res) {
        int N = req.has_param("window") ? std::stoi(req.get_param_value("window")) : 7;
        if (N < 1 || N > 14) throw std::invalid_argument("Window must be between 1 and 14 days");
        sendJson(res, app.prediction.toJson(N));
    }));

    // Dijkstra from canteen, then Merge Sort by distance
    svr.Get("/api/nearest", guarded([&](const httplib::Request&, httplib::Response& res) {
        auto r = app.graph.dijkstra("CANTEEN");
        struct Row { std::string id, name; double dist; std::vector<std::string> path; };
        std::vector<Row> rows;
        for (const NGO& n : app.ngos.all()) {
            int idx = app.graph.indexOf(n.getId());
            double d = (idx >= 0) ? r.dist[idx] : std::numeric_limits<double>::infinity();
            rows.push_back(Row{n.getId(), n.getName(), d,
                               idx >= 0 && std::isfinite(d) ? app.graph.pathTo(r, idx) : std::vector<std::string>{}});
        }
        mergeSort(rows.data(), static_cast<int>(rows.size()), [](const Row& a, const Row& b) { return a.dist < b.dist; });
        json arr = json::array();
        for (const Row& row : rows) {
            json j = {{"ngoId", row.id}, {"name", row.name}, {"path", row.path}};
            j["distanceKm"] = std::isfinite(row.dist) ? json(round2(row.dist)) : json(nullptr);
            arr.push_back(j);
        }
        engineLog().add("Graph", "Dijkstra from CANTEEN over " + std::to_string(app.graph.nodeCount()) + " nodes");
        sendJson(res, json{{"items", arr}});
    }));

    svr.Get("/api/landmarks", guarded([&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (int i = 0; i < app.graph.nodeCount(); i++) {
            const std::string& nm = app.graph.nameAt(i);
            if (nm != "CANTEEN" && !app.ngos.findNGO(nm)) arr.push_back(nm);
        }
        sendJson(res, json{{"items", arr}});
    }));

    // Dashboard numbers
    svr.Get("/api/stats", guarded([&](const httplib::Request&, httplib::Response& res) {
        double availQty = 0, availValue = 0;
        int urgent = 0, expired = 0, items = 0;
        app.inventory.forEach([&](FoodItem* f) {
            if (f->isExpired()) { expired++; return; }
            items++;
            availQty += f->getQuantity();
            availValue += f->getQuantity() * f->getPrice();
            std::string lv = f->urgencyLevel();
            if (lv == "CRITICAL" || lv == "HIGH") urgent++;
        });
        int confirmed = 0, rejected = 0, distributed = 0;
        double savedQty = 0, revenue = 0;
        app.log.forEach([&](const Order& o) {
            if (o.countsAsSaved()) { confirmed++; savedQty += o.getQuantity(); revenue += o.getAmount(); }
            if (o.getStatus() == "REJECTED") rejected++;
            if (o.getStatus() == "DISTRIBUTED") distributed++;
        });
        double accRaw = 0, accPrep = 0;
        app.prediction.backtest("RAW", 7, accRaw);
        app.prediction.backtest("PREPARED", 7, accPrep);

        sendJson(res, json{
            {"availableItems", items}, {"availableQty", round2(availQty)}, {"availableValue", round2(availValue)},
            {"urgentItems", urgent}, {"expiredItems", expired},
            {"pendingOrders", app.queue.size()}, {"transactions", app.log.size()},
            {"confirmed", confirmed}, {"rejected", rejected}, {"distributed", distributed},
            {"foodSavedKg", round2(savedQty)}, {"mealsEquivalent", static_cast<int>(savedQty / 0.4)},
            {"revenue", round2(revenue)}, {"ngoCount", app.ngos.size()},
            {"predictionAccuracy", round2((accRaw + accPrep) / 2)},
            {"structures", {
                {"linkedList", app.inventory.size()}, {"minHeap", app.inventory.heapSize()},
                {"queue", app.queue.size()}, {"stack", app.log.size()},
                {"hashTable", app.ngos.size()}, {"hashBuckets", app.ngos.buckets()},
                {"historyDays", app.prediction.daysStored()}, {"graphNodes", app.graph.nodeCount()}}}});
    }));

    svr.Get("/api/log", guarded([&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{{"items", engineLog().toJson()}});
    }));
}

// Console demo for viva: run with --demo
inline int runDemo(App& app) {
    std::cout << "\n=== Inventory (LinkedList, overloaded operator<<) ===\n";
    app.inventory.forEach([](FoodItem* f) { std::cout << *f << "\n"; });

    std::cout << "\n=== Top 3 most urgent (MinHeap) ===\n";
    for (FoodItem* f : app.inventory.getMostUrgent(3)) std::cout << *f << "\n";

    std::cout << "\n=== Hash lookup N103 ===\n";
    if (NGO* n = app.ngos.findNGO("N103"))
        std::cout << n->getName() << "  (bucket " << app.ngos.bucketOf("N103") << ")\n";

    std::cout << "\n=== Nearest NGOs (Dijkstra) ===\n";
    auto r = app.graph.dijkstra("CANTEEN");
    for (const NGO& n : app.ngos.all()) {
        int idx = app.graph.indexOf(n.getId());
        if (idx < 0) continue;
        std::cout << n.getId() << " " << n.getName() << " : " << r.dist[idx] << " km  via ";
        for (auto& s : app.graph.pathTo(r, idx)) std::cout << s << " ";
        std::cout << "\n";
    }

    std::cout << "\n=== Prediction (7-day moving average) ===\n";
    std::cout << "RAW tomorrow      : " << round2(app.prediction.predictNextDay("RAW")) << " kg\n";
    std::cout << "PREPARED tomorrow : " << round2(app.prediction.predictNextDay("PREPARED")) << " kg\n";
    std::cout << "\n(demo mode does not save changes)\n";
    return 0;
}
