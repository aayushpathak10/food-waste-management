// ===== PERSON 1 : main() - start-up, load/seed data, start server =====
#include "routes_food.hpp"
#include "routes_ngo_orders.hpp"
#include "routes_insights.hpp"

static bool fileExists(const std::string& p) { std::ifstream f(p); return static_cast<bool>(f); }

int main(int argc, char* argv[]) {
    int port = 8080;
    bool demo = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--demo") demo = true;
        else if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    }

    // Works from the project root or from backend/
    std::string root = fileExists("frontend/index.html") ? "." : (fileExists("../frontend/index.html") ? ".." : ".");
    std::string frontendDir = root + "/frontend";
    std::string dataDir = root + "/data";
    MAKE_DIR(dataDir.c_str());

    App app(dataDir);
    if (app.store.exists("foods.txt")) {
        app.loadAll();
        std::cout << "Loaded data from " << dataDir << "\n";
    } else {
        app.seed();
        if (!demo) app.saveAll();
        std::cout << "First run: created sample data in " << dataDir << "\n";
    }
    if (demo) return runDemo(app);

    httplib::Server svr;
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                             {"Access-Control-Allow-Headers", "Content-Type"},
                             {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"}});
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    registerFoodRoutes(svr, app);        // Person 2
    registerNgoOrderRoutes(svr, app);    // Person 3
    registerInsightRoutes(svr, app);     // Person 4

    if (!svr.set_mount_point("/", frontendDir)) {
        std::cerr << "Could not find the frontend folder at " << frontendDir << "\n";
        return 1;
    }
    std::cout << "\n  FoodBridge is running ->  http://localhost:" << port << "\n  Press Ctrl+C to stop.\n\n";
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Could not start server on port " << port << " (try --port 9090)\n";
        return 1;
    }
    return 0;
}
