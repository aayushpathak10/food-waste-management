// ===== PERSON 1 : common helpers (shared by everyone) =====
#pragma once
// KEEP your ORIGINAL #include lines here (httplib, json, mutex, fstream, cmath, limits,
// and all your own headers). Also keep your "using json = nlohmann::json;" line.

inline std::mutex appMutex;   // only one request runs at a time (thread safety)

inline void sendJson(httplib::Response& res, const json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(), "application/json; charset=utf-8");
}
inline void sendError(httplib::Response& res, const std::string& msg, int status = 400) {
    sendJson(res, json{{"error", msg}}, status);
}
inline json body(const httplib::Request& req) {
    return req.body.empty() ? json::object() : json::parse(req.body);
}
inline double numField(const json& j, const char* key, double def = 0) {
    if (!j.contains(key) || j[key].is_null()) return def;
    if (j[key].is_number()) return j[key].get<double>();
    if (j[key].is_string()) {
        std::string s = trim(j[key].get<std::string>());
        if (s.empty()) return def;
        try { return std::stod(s); } catch (...) {}
    }
    throw std::invalid_argument(std::string("Field '") + key + "' must be a number");
}
inline std::string strField(const json& j, const char* key, const std::string& def = "") {
    if (!j.contains(key) || j[key].is_null()) return def;
    return j[key].is_string() ? j[key].get<std::string>() : j[key].dump();
}

// Wraps every handler: locks the mutex and turns exceptions into JSON errors
template <typename F>
httplib::Server::Handler guarded(F f) {
    return [f](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(appMutex);
        try { f(req, res); }
        catch (const json::exception& e)     { sendError(res, std::string("Invalid JSON: ") + e.what(), 400); }
        catch (const std::invalid_argument& e) { sendError(res, e.what(), 400); }
        catch (const std::exception& e)      { sendError(res, e.what(), 500); }
    };
}
