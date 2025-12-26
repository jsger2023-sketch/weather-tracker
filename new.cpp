#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
using namespace std;

// -----------------------
// Linked List for Favorites
// -----------------------
struct FavNode {
    string city;
    FavNode* next;
    FavNode(string c) : city(c), next(nullptr) {}
};

class FavoriteList {
    FavNode* head = nullptr;
public:
    void addCity(const string& city) {
        if (exists(city)) {
            cout << "Already in favorites.\n";
            return;
        }
        FavNode* node = new FavNode(city);
        node->next = head;
        head = node;
        cout << "Added " << city << " to favorites.\n";
    }

    bool exists(const string& city) {
        for (FavNode* p = head; p; p = p->next)
            if (p->city == city)
                return true;
        return false;
    }

    void showFavorites() {
        if (!head) {
            cout << "No favorites yet.\n";
            return;
        }
        cout << "\n=== Favorite Cities ===\n";
        for (FavNode* p = head; p; p = p->next)
            cout << "- " << p->city << "\n";
    }
};

// -----------------------
// Binary Search Tree for Forecast
// -----------------------
struct DaySummary {
    string date;
    double minTemp;
    double maxTemp;
    string desc;
};

struct BSTNode {
    DaySummary data;
    BSTNode* left;
    BSTNode* right;
    BSTNode(DaySummary d) : data(d), left(nullptr), right(nullptr) {}
};

class ForecastBST {
    BSTNode* root = nullptr;

    void insert(BSTNode*& node, const DaySummary& d) {
        if (!node)
            node = new BSTNode(d);
        else if (d.date < node->data.date)
            insert(node->left, d);
        else
            insert(node->right, d);
    }

    void inorder(BSTNode* node) {
        if (!node) return;
        inorder(node->left);
        cout << node->data.date << " : "
             << fixed << setprecision(1)
             << node->data.minTemp << "°C — "
             << node->data.maxTemp << "°C, "
             << node->data.desc << "\n";
        inorder(node->right);
    }

public:
    void addDay(const DaySummary& d) { insert(root, d); }
    void showForecast() {
        if (!root) {
            cout << "No forecast data.\n";
            return;
        }
        cout << "\n=== 5-Day Forecast ===\n";
        inorder(root);
    }
};

// -----------------------
// Main Menu
// -----------------------
int main() {
    FavoriteList favorites;
    ForecastBST forecast;

    // Preload some fake data for simplicity
    vector<DaySummary> sampleDays = {
        {"2025-10-21", 22.5, 31.2, "clear sky"},
        {"2025-10-22", 24.0, 33.5, "light rain"},
        {"2025-10-23", 21.8, 30.1, "scattered clouds"},
        {"2025-10-24", 23.2, 34.5, "sunny"},
        {"2025-10-25", 25.0, 35.8, "hot and dry"}
    };
    for (auto& d : sampleDays) forecast.addDay(d);

    int choice;
    while (true) {
        cout << "\n========== WEATHER CLI ==========\n";
        cout << "1) Add Favorite City\n";
        cout << "2) Show Favorite Cities\n";
        cout << "3) Show 5-Day Forecast\n";
        cout << "4) Exit\n";
        cout << "=================================\n";
        cout << "Enter choice: ";
        cin >> choice;
        cin.ignore();

        if (choice == 1) {
            cout << "Enter city name: ";
            string city;
            getline(cin, city);
            favorites.addCity(city);
        }
        else if (choice == 2) {
            favorites.showFavorites();
        }
        else if (choice == 3) {
            forecast.showForecast();
        }
        else if (choice == 4) {
            cout << "Goodbye!\n";
            break;
        }
        else {
            cout << "Invalid choice.\n";
        }
    }

    return 0;
}
