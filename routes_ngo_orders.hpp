// ===== PERSON 3 : NGO + Orders + Transactions (Hash Table, Queue, Stack, Matching Engine) =====
#pragma once
#include "app.hpp"

inline void registerNgoOrderRoutes(httplib::Server& svr, App& app) {
    // ---- NGOs (Hash Table) ----
    svr.Get("/api/ngos", guarded([&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        std::string today = fmtDate(nowTs());
        for (const NGO& n : app.ngos.all()) {
            json j = n.toJson();
            j["usedToday"] = round2(app.log.confirmedQtyOn(n.getId(), today));
            j["bucket"] = app.ngos.bucketOf(n.getId());
            arr.push_back(j);
        }
        sendJson(res, json{{"items", arr}});
    }));

    svr.Get(R"(/api/ngos/(\w+))", guarded([&](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        NGO* n = app.ngos.findNGO(id);                 // O(1) hash lookup
        if (!n) return sendError(res, "NGO not found", 404);
        json hist = json::array(), pending = json::array();
        for (const Order& o : n->viewHistory(app.log)) hist.push_back(o.toJson());
        for (const Order& o : app.queue.snapshot()) if (o.getNgoId() == id) pending.push_back(o.toJson());
        json j = n->toJson();
        j["usedToday"] = round2(app.log.confirmedQtyOn(id, fmtDate(nowTs())));
        j["bucket"] = app.ngos.bucketOf(id);
        sendJson(res, json{{"ngo", j}, {"history", hist}, {"pending", pending}});
    }));

    // Register NGO; if a landmark is given, add an edge in the map graph
    svr.Post("/api/ngos", guarded([&](const httplib::Request& req, httplib::Response& res) {
        json b = body(req);
        std::string id = app.ngos.generateId();
        NGO n(id, strField(b, "name"), strField(b, "contactNumber"), strField(b, "address"), numField(b, "capacityPerDay"));
        std::string landmark = strField(b, "landmark");
        double dist = numField(b, "distanceKm", 0);
        if (!landmark.empty() && dist <= 0) throw std::invalid_argument("Distance from landmark must be greater than zero");
        app.ngos.registerNGO(n);
        if (!landmark.empty()) {
            app.graph.addEdge(id, landmark, dist);
            engineLog().add("Graph", "Added edge " + id + " <-> " + landmark + " (" + num(dist) + " km)");
        }
        app.saveAll();
        sendJson(res, json{{"ngo", n.toJson()}, {"bucket", app.ngos.bucketOf(id)}}, 201);
    }));

    // ---- Orders (Queue - FIFO) ----
    svr.Post("/api/order", guarded([&](const httplib::Request& req, httplib::Response& res) {
        json b = body(req);
        NGO* ngo = app.ngos.findNGO(strField(b, "ngoId"));
        if (!ngo) throw std::invalid_argument("Unknown NGO ID");
        FoodItem* item = app.inventory.findItem(strField(b, "foodItemId"));
        if (!item) throw std::invalid_argument("Food item not found");
        Order o = ngo->placeOrder(app.queue.generateId(), *item, numField(b, "quantity"));
        app.queue.enqueueOrder(o);
        app.saveAll();
        sendJson(res, json{{"order", o.toJson()}, {"queuePosition", app.queue.size()}}, 201);
    }));

    svr.Get("/api/queue", guarded([&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        int pos = 1;
        for (const Order& o : app.queue.snapshot()) { json j = o.toJson(); j["position"] = pos++; arr.push_back(j); }
        sendJson(res, json{{"items", arr}});
    }));

    // Matching engine takes orders from the queue and confirms / rejects them
    svr.Post("/api/queue/process", guarded([&](const httplib::Request&, httplib::Response& res) {
        json arr = json::array();
        for (const Order& o : app.engine.processQueue()) arr.push_back(o.toJson());
        app.saveAll();
        sendJson(res, json{{"processed", arr}});
    }));

    // ---- Transactions (Stack - LIFO) ----
    svr.Get("/api/transactions", guarded([&](const httplib::Request& req, httplib::Response& res) {
        int n = req.has_param("n") ? std::stoi(req.get_param_value("n")) : -1;
        json arr = json::array();
        for (const Order& o : app.log.getRecent(n)) arr.push_back(o.toJson());
        sendJson(res, json{{"items", arr}, {"stackDepth", app.log.size()}});
    }));

    // POST /api/transactions/O1006/status { status: PICKED_UP | DISTRIBUTED }
    svr.Post(R"(/api/transactions/(\w+)/status)", guarded([&](const httplib::Request& req, httplib::Response& res) {
        json b = body(req);
        app.log.updateStatus(req.matches[1], strField(b, "status"));
        app.saveAll();
        sendJson(res, app.log.find(req.matches[1])->toJson());
    }));
}
