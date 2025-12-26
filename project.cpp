#include <bits/stdc++.h>
#include <curl/curl.h>
#include "json.hpp"  // nlohmann/json (https://github.com/nlohmann/json)

using json = nlohmann::json;
using namespace std;

// -----------------------
// Utils
// -----------------------
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* out) {
    out->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string urlEncode(const string& s) {
    string r;
    for (unsigned char c : s) {
        if (isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') r.push_back(c);
        else if (c==' ') r += "%20";
        else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            r += buf;
        }
    }
    return r;
}

string getenvSafe(const char* key) {
    const char* v = std::getenv(key);
    return v ? string(v) : string();
}

string httpGet(const string& url) {
    CURL* curl = nullptr;
    CURLcode res;
    string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        cerr << "[Error] curl_easy_init() failed\n";
        curl_global_cleanup();
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (WeatherCLI)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        cerr << "[HTTP Error] " << curl_easy_strerror(res) << "\n";
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return response;
}

inline double KtoC(double K) { return K - 273.15; }

string toFixed(double x, int p=1) {
    ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << setprecision(p) << x;
    return oss.str();
}

// -----------------------
// Weather logic
// -----------------------
struct CurrentWeather {
    string city;
    string desc;
    double tempC = NAN, feelsC = NAN;
    int humidity = 0;
    double windKmh = 0.0;
};

struct DaySummary {
    string date; // YYYY-MM-DD
    double minC = +1e9;
    double maxC = -1e9;
    string mainDesc;
};

bool parseCurrentWeather(const string& raw, CurrentWeather& out) {
    try {
        auto j = json::parse(raw);
        if (!j.contains("main") || !j.contains("weather")) return false;

        out.city = j.value("name", "");
        auto main = j["main"];
        auto weatherArr = j["weather"];
        auto wind = j.value("wind", json::object());

        out.tempC = KtoC(main.value("temp", 0.0));
        out.feelsC = KtoC(main.value("feels_like", 0.0));
        out.humidity = main.value("humidity", 0);

        double windMs = wind.value("speed", 0.0);
        out.windKmh = windMs * 3.6;

        if (!weatherArr.empty()) {
            out.desc = weatherArr[0].value("description", "");
        }

        return true;
    } catch (...) {
        return false;
    }
}

string pickRepresentativeDesc(const vector<json>& entries) {
    int bestIdx = -1;
    long bestDelta = LONG_MAX;
    for (size_t i=0;i<entries.size();++i) {
        string dtTxt = entries[i].value("dt_txt", "");
        if (dtTxt.size() >= 13) {
            string hh = dtTxt.substr(11,2);
            long delta = labs(stol(hh) - 12);
            if (delta < bestDelta) {
                bestDelta = delta;
                bestIdx = (int)i;
            }
        }
    }
    if (bestIdx >= 0) {
        auto wArr = entries[bestIdx].value("weather", json::array());
        if (!wArr.empty()) return wArr[0].value("description", "");
    }
    return "—";
}

bool parseForecastDaily(const string& raw, vector<DaySummary>& daysOut) {
    try {
        auto j = json::parse(raw);
        if (!j.contains("list")) return false;

        unordered_map<string, vector<json>> byDate;
        for (auto& item : j["list"]) {
            string dtTxt = item.value("dt_txt", "");
            if (dtTxt.size() < 10) continue;
            string day = dtTxt.substr(0,10);
            byDate[day].push_back(item);
        }

        vector<string> keys;
        keys.reserve(byDate.size());
        for (auto& kv : byDate) keys.push_back(kv.first);
        sort(keys.begin(), keys.end());

        for (auto& day : keys) {
            DaySummary d;
            d.date = day;
            auto& vec = byDate[day];
            for (auto& e : vec) {
                auto main = e.value("main", json::object());
                double t = KtoC(main.value("temp", 0.0));
                d.minC = min(d.minC, t);
                d.maxC = max(d.maxC, t);
            }
            d.mainDesc = pickRepresentativeDesc(vec);
            daysOut.push_back(d);
            if (daysOut.size() == 5) break;
        }
        return !daysOut.empty();
    } catch (...) {
        return false;
    }
}

vector<string> computeAlerts(double tempC, const string& desc) {
    vector<string> alerts;
    if (tempC >= 40.0) alerts.push_back("🔥 Heatwave Alert");
    if (tempC <= 5.0) alerts.push_back("❄️ Coldwave Alert");

    string d = desc;
    transform(d.begin(), d.end(), d.begin(), ::tolower);
    if (d.find("rain") != string::npos || d.find("drizzle") != string::npos || d.find("thunder") != string::npos)
        alerts.push_back("☔ Rain Alert — carry an umbrella");

    return alerts;
}

// -----------------------
// Favorites
// -----------------------
const string FAVORITES_FILE = "favorites.txt";

vector<string> loadFavorites() {
    vector<string> v;
    ifstream in(FAVORITES_FILE);
    string line;
    while (getline(in, line)) {
        if (!line.empty()) v.push_back(line);
    }
    return v;
}

void addFavorite(const string& city) {
    auto favs = loadFavorites();
    if (find(favs.begin(), favs.end(), city) != favs.end()) {
        cout << "Already in favorites.\n";
        return;
    }
    ofstream out(FAVORITES_FILE, ios::app);
    out << city << "\n";
    cout << "Added to favorites.\n";
}

// -----------------------
// API wrappers
// -----------------------
string buildCurrentUrl(const string& city, const string& apiKey) {
    return "https://api.openweathermap.org/data/2.5/weather?q=" + urlEncode(city) + "&appid=" + apiKey;
}

string buildForecastUrl(const string& city, const string& apiKey) {
    return "https://api.openweathermap.org/data/2.5/forecast?q=" + urlEncode(city) + "&appid=" + apiKey;
}

// -----------------------
// Show weather functions
// -----------------------
void showCurrentWeather(const string& apiKey) {
    cout << "\nEnter city: " << flush;
    string city;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    getline(cin, city);

    string data = httpGet(buildCurrentUrl(city, apiKey));
    CurrentWeather cw;
    if (!parseCurrentWeather(data, cw)) {
        cout << "⚠️ Could not fetch current weather. Check city/API key.\n";
        return;
    }

    cout << "\n=== Current Weather: " << cw.city << " ===\n";
    cout << "Condition : " << cw.desc << "\n";
    cout << "Temp : " << toFixed(cw.tempC) << "°C (feels " << toFixed(cw.feelsC) << "°C)\n";
    cout << "Humidity : " << cw.humidity << "%\n";
    cout << "Wind : " << toFixed(cw.windKmh) << " km/h\n";

    auto alerts = computeAlerts(cw.tempC, cw.desc);
    if (!alerts.empty()) {
        cout << "Alerts : ";
        for (size_t i=0;i<alerts.size();++i) {
            if (i) cout << " | ";
            cout << alerts[i];
        }
        cout << "\n";
    }

    cout << "\nAdd to favorites? (y/n): " << flush;
    char ch;
    cin >> ch;
    if (ch=='y' || ch=='Y') addFavorite(city);
}

void showForecast(const string& apiKey) {
    cout << "\nEnter city: " << flush;
    string city;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    getline(cin, city);

    vector<DaySummary> days;
    string data = httpGet(buildForecastUrl(city, apiKey));
    if (!parseForecastDaily(data, days)) {
        cout << "⚠️ Could not fetch forecast. Check city/API key.\n";
        return;
    }

    cout << "\n=== 5-Day Forecast: " << city << " ===\n";
    for (auto& d : days) {
        cout << d.date << " : " << toFixed(d.minC) << "°C — " << toFixed(d.maxC) << "°C, " << d.mainDesc << "\n";
    }
}

void showFavoritesWeather(const string& apiKey) {
    auto favs = loadFavorites();
    if (favs.empty()) {
        cout << "No favorites yet. Add some from Current Weather.\n";
        return;
    }

    cout << "\n=== Favorites (Current Weather) ===\n";
    for (auto& city : favs) {
        string data = httpGet(buildCurrentUrl(city, apiKey));
        CurrentWeather cw;
        if (parseCurrentWeather(data, cw)) {
            cout << "- " << setw(18) << left << city << " " << toFixed(cw.tempC) << "°C, " << cw.desc << "\n";
        } else {
            cout << "- " << city << " : (fetch failed)\n";
        }
    }
}

// -----------------------
// Main menu
// -----------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string apiKey = getenvSafe("OPENWEATHER_API_KEY");
    if (apiKey.empty()) {
        cerr << "ERROR: Set environment variable OPENWEATHER_API_KEY first.\n";
        cerr << "Get a free key at https://openweathermap.org/api\n";
        return 1;
    }

    while (true) {
        cout << "\n========== WEATHER CLI ==========\n";
        cout << "1) Current Weather by City\n";
        cout << "2) 5-Day Forecast (Daily Summary)\n";
        cout << "3) Favorites: Show All\n";
        cout << "4) Exit\n";
        cout << "=================================\n";
        cout << "Enter choice: " << flush;

        int choice;
        if (!(cin >> choice)) break;

        switch (choice) {
            case 1: showCurrentWeather(apiKey); break;
            case 2: showForecast(apiKey); break;
            case 3: showFavoritesWeather(apiKey); break;
            case 4: cout << "Goodbye!\n"; return 0;
            default: cout << "Invalid choice.\n";
        }
    }
    return 0;
}

