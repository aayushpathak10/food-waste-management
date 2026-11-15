// ===== PERSON 1 : App object, save/load, sample data =====
#pragma once
#include "common.hpp"

struct App {
    Canteen canteen{"C01", "Main Campus Canteen", "Block A, Ground Floor"};
    InventoryManager inventory;   // LinkedList + MinHeap
    NGOManager ngos;              // Hash table
    OrderQueue queue;             // Queue (FIFO)
    TransactionLog log;           // Stack
    PredictionEngine prediction;  // Moving average
    Graph graph;                  // Dijkstra
    DataStore store;              // File saving
    MatchingEngine engine{inventory, ngos, queue, log};

    explicit App(const std::string& dataDir) : store(dataDir) {}

    void saveAll() {
        store.saveFoods(inventory);   store.saveNGOs(ngos);
        store.saveTransactions(log);  store.savePending(queue);
        store.saveHistory(prediction); store.saveGraph(graph);
    }
    void loadAll() {
        store.loadNGOs(ngos);         store.loadFoods(inventory);
        store.loadTransactions(log, queue); store.loadPending(queue);
        store.loadHistory(prediction); store.loadGraph(graph);
    }

    // First run only: creates sample data relative to "now"
    void seed() {
        time_t now = nowTs();
        auto hrs = [&](double h) { return now - static_cast<time_t>(h * 3600); };

        // NGOs
        struct N { const char *id, *name, *phone, *addr; double cap; };
        const N ngoList[] = {
            {"N101", "Annapurna Seva Trust",  "9810011122", "Sector 37, near Gate 2", 40},
            {"N102", "Roti Ghar Foundation",  "9873345566", "Market Chowk, Old Town", 25},
            {"N103", "Asha Community Kitchen","9999012345", "Behind Metro Station",   30},
            {"N104", "Saathi Welfare Society","9650987654", "Shanti Nagar Colony",    15}};
        for (const N& n : ngoList) ngos.registerNGO(NGO(n.id, n.name, n.phone, n.addr, n.cap), true);

        // Raw food: id, name, qty, unit, price, canteen, addedTime, storage, shelfLifeDays
        inventory.addFood(new RawFood("F101", "Tomatoes", 8, "kg", 20, "C01", hrs(30), "Room", 3), true);
        inventory.addFood(new RawFood("F102", "Spinach", 4, "kg", 15, "C01", hrs(38), "Room", 2), true);
        inventory.addFood(new RawFood("F103", "Milk", 6, "litre", 40, "C01", hrs(20), "Refrigerated", 2), true);
        inventory.addFood(new RawFood("F104", "Paneer", 2, "kg", 180, "C01", hrs(10), "Refrigerated", 3), true);
        inventory.addFood(new RawFood("F105", "Basmati Rice (uncooked)", 15, "kg", 35, "C01", hrs(48), "Room", 30), true);
        inventory.addFood(new RawFood("F106", "Potatoes", 12, "kg", 18, "C01", hrs(24), "Room", 10), true);
        // Prepared food: ..., addedTime, cookedTime, consumeWithinHours
        inventory.addFood(new PreparedFood("F107", "Veg Biryani", 10, "kg", 60, "C01", hrs(1.5), hrs(2), 6), true);
        inventory.addFood(new PreparedFood("F108", "Rajma Curry", 4, "kg", 50, "C01", hrs(3.5), hrs(4.5), 6), true);
        inventory.addFood(new PreparedFood("F109", "Dal Makhani", 6, "kg", 45, "C01", hrs(0.5), hrs(1), 8), true);
        inventory.addFood(new PreparedFood("F110", "Chapati", 5, "kg", 30, "C01", hrs(1), hrs(1.5), 10), true);

        // 28 days of waste history (weekends cook less)
        const double rawByDay[7]  = {11, 24, 22, 26, 23, 27, 15};   // Sun..Sat
        const double prepByDay[7] = {14, 32, 29, 35, 31, 38, 20};
        for (int d = 28; d >= 1; d--) {
            time_t t = now - static_cast<time_t>(d) * 86400;
            int wd = std::localtime(&t)->tm_wday;
            double wobble = ((d * 7) % 5) - 2;
            prediction.recordDailyWaste("RAW", fmtDate(t), rawByDay[wd] + wobble, true);
            prediction.recordDailyWaste("PREPARED", fmtDate(t), prepByDay[wd] - wobble * 1.5, true);
        }
        inventory.forEach([&](FoodItem* f) {
            if (fmtDate(f->getDateAdded()) == fmtDate(now))
                prediction.recordDailyWaste(f->getType(), fmtDate(now), f->getQuantity(), true);
        });

        // Old completed transactions (oldest first)
        auto past = [&](double days) { return std::to_string(static_cast<long long>(now - days * 86400)); };
        const std::string seedTx[] = {
            "O1001|F090|Veg Pulao|PREPARED|N101|Annapurna Seva Trust|8|kg|400|DISTRIBUTED||" + past(3) + "|" + past(2.9),
            "O1002|F091|Onions|RAW|N102|Roti Ghar Foundation|10|kg|200|DISTRIBUTED||" + past(2.5) + "|" + past(2.4),
            "O1003|F092|Chole|PREPARED|N104|Saathi Welfare Society|20|kg|0|REJECTED|Exceeds NGO daily capacity (0 of 15 used)|" + past(2) + "|" + past(2),
            "O1004|F093|Aloo Sabzi|PREPARED|N103|Asha Community Kitchen|7|kg|280|DISTRIBUTED||" + past(1.2) + "|" + past(1.1),
            "O1005|F094|Bananas|RAW|N101|Annapurna Seva Trust|6|kg|180|PICKED_UP||" + past(0.9) + "|" + past(0.8)};
        for (const std::string& line : seedTx) {
            Order o = Order::deserialize(line);
            queue.bumpId(o.getOrderId());
            log.logTransaction(o, true);
        }

        // Map: road distances in km
        struct E { const char *a, *b; double km; };
        const E edges[] = {{"CANTEEN","Gate-2",1.2}, {"CANTEEN","Metro-Station",3.5}, {"Gate-2","Market-Chowk",2.8},
                           {"Gate-2","N101",0.8}, {"Market-Chowk","N102",0.6}, {"Metro-Station","Market-Chowk",1.5},
                           {"Metro-Station","N103",0.4}, {"Metro-Station","Shanti-Nagar",2.6},
                           {"Shanti-Nagar","N104",0.9}, {"Gate-2","Shanti-Nagar",5.5}};
        for (const E& e : edges) graph.addEdge(e.a, e.b, e.km);
    }
};
