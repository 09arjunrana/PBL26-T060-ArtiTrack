// m1.cpp - CENTRAL HUB: login -> artifact menu (this is the ONLY file with main)
// Build (2 steps - m4.c is PURE C, so it must be compiled with gcc, not g++):
//   gcc -std=c11 -Wall -Wextra -c m4.c -o m4.o
//   g++ -std=c++17 -Wall -Wextra m1.cpp m4.o -o artitrack
// m0.cpp (login) and m2.cpp (classes) are pulled in by #includes;
// m4.c (movement tracking, C) is linked in as m4.o and talks to this file
// through the small bridge_* functions below (declared in m4.h).
#include "m0.cpp"
#include "m2.cpp"
#include "m4.h"      // C interface of Module 4 (m4.c)
#include <iomanip>
#include <cstdio>
#include <cstring>
using namespace std;

// ================== Saving / loading ==================
class ArtifactStore {
private:
    string filename;
    vector<unique_ptr<Artifact>> items;

    // Rewrites the whole save file (write to a temp file, then swap it in)
    bool saveAll() const
    {
        string tmp = filename + ".tmp";
        {
            ofstream out(tmp, ios::trunc);
            if (!out.is_open()) return false;
            for (const auto& a : items) out << a->serialize() << "\n";
            out.flush();
            if (!out) return false;
        }
        std::remove(filename.c_str());
        return std::rename(tmp.c_str(), filename.c_str()) == 0;
    }

public:
    explicit ArtifactStore(const string& file = "artifacts.txt") : filename(file) {}

    // Read everything saved by earlier sessions
    void load()
    {
        items.clear();
        ifstream in(filename);
        if (!in.is_open()) return;          // first run: nothing saved yet

        string line;
        int skipped = 0;
        while (getline(in, line))
        {
            if (line.empty()) continue;
            unique_ptr<Artifact> a = parseArtifact(line);
            if (a && !exists(a->getId())) items.push_back(std::move(a));
            else skipped++;
        }
        if (skipped > 0)
        {
            cout << "Warning: skipped " << skipped
                 << " unreadable or duplicate line(s) in " << filename << ".\n";
            // Module 4 rewrites the whole file, which would erase those lines,
            // so keep a copy of the original first.
            ifstream src(filename, ios::binary);
            ofstream bak(filename + ".bak", ios::binary | ios::trunc);
            if (src && bak)
            {
                bak << src.rdbuf();
                cout << "A backup was saved as " << filename << ".bak\n";
            }
        }
    }

    bool exists(const string& id) const { return find(id) != nullptr; }

    const Artifact* find(const string& id) const
    {
        for (const auto& a : items)
            if (a->getId() == id) return a.get();
        return nullptr;
    }

    // Saves to disk first; only keeps the artifact in memory if the save worked
    bool add(unique_ptr<Artifact> a)
    {
        ofstream out(filename, ios::app);
        if (!out.is_open()) return false;
        out << a->serialize() << "\n";
        out.flush();
        if (!out) return false;
        items.push_back(std::move(a));
        return true;
    }

    // ---- used by Module 4 (movement tracking) ----
    Artifact* findMutable(const string& id)
    {
        for (auto& a : items)
            if (a->getId() == id) return a.get();
        return nullptr;
    }

    // Changes an artifact's location in memory AND on disk.
    // If the save fails the old location is restored and false is returned.
    bool updateLocation(const string& id, const string& newLoc)
    {
        Artifact* a = findMutable(id);
        if (!a) return false;
        string oldLoc = a->getLocation();
        a->setLocation(newLoc);
        if (saveAll()) return true;
        a->setLocation(oldLoc);
        return false;
    }

    size_t size() const { return items.size(); }

    void displayAll() const
    {
        if (items.empty())
        {
            cout << "No artifacts saved yet.\n";
            return;
        }
        cout << "\n=== " << items.size() << " artifact(s) ===\n";
        for (const auto& a : items)
        {
            a->display();
            cout << "  Priority score: " << fixed << setprecision(2)
                 << a->calculatePriority() << defaultfloat << "\n";
            cout << "-----\n";
        }
    }
};

// ---------- Bridge for Module 4 (m4.c is pure C and cannot see ArtifactStore) ----------
// artifactMenu() points g_store at its store while the movement menu is open.
static ArtifactStore* g_store = nullptr;

extern "C" int bridge_findArtifact(const char* id,
                                   char* name, size_t nameSize,
                                   char* location, size_t locationSize)
{
    if (!g_store) return 0;
    const Artifact* a = g_store->find(id);
    if (!a) return 0;
    snprintf(name, nameSize, "%s", a->getName().c_str());
    snprintf(location, locationSize, "%s", a->getLocation().c_str());
    return 1;
}

extern "C" int bridge_updateLocation(const char* id, const char* newLocation)
{
    if (!g_store) return 0;
    return g_store->updateLocation(id, newLocation) ? 1 : 0;
}

// ---------- Artifact entry ----------
void addArtifact(ArtifactStore& store)
{
    cout << "\n--- Add Artifact ---\n"
         << "1. Pottery\n2. Coin\n3. Ancient Coin\n";
    int type = readInt("Select type: ", 1, 3);

    string id;
    while (true)
    {
        id = readLine("Enter Artifact ID (e.g. P001): ");
        if (!store.exists(id)) break;
        cout << "An artifact with this ID already exists!\n";
    }

    string name  = readLine("Enter Name: ");
    int age      = readInt("Enter Estimated Age (years): ", 0, 100000);
    double cond  = readDouble("Enter Condition Score (0 = ruined, 10 = perfect): ", 0.0, 10.0);
    string loc   = readLine("Enter Current Location: ");

    unique_ptr<Artifact> a;
    if (type == 1)
    {
        double por = readDouble("Enter Porosity Index (0.0 - 1.0): ", 0.0, 1.0);
        a = make_unique<PotteryArtifact>(id, name, age, cond, loc, por);
    }
    else
    {
        cout << "Metal: 1. Gold  2. Silver  3. Bronze  4. Copper\n";
        static const char* metals[] = {"Gold", "Silver", "Bronze", "Copper"};
        string metal = metals[readInt("Select metal: ", 1, 4) - 1];

        if (type == 2)
            a = make_unique<CoinArtifact>(id, name, age, cond, loc, metal);
        else
        {
            string dynasty = readLine("Enter Dynasty (e.g. Maurya, Gupta): ");
            a = make_unique<AncientCoin>(id, name, age, cond, loc, metal, dynasty);
        }
    }

    if (store.add(std::move(a)))
        cout << "Artifact saved successfully!\n";
    else
        cout << "ERROR: could not write to the save file. Artifact NOT saved.\n";
}

void searchArtifact(const ArtifactStore& store)
{
    string id = readLine("Enter Artifact ID to search: ");
    const Artifact* a = store.find(id);
    if (!a)
    {
        cout << "Artifact not found.\n";
        return;
    }
    a->display();
    cout << "  Priority score: " << a->calculatePriority() << "\n";
}

// ---------- Artifact menu (after login) ----------
void artifactMenu(const string& user)
{
    ArtifactStore store;
    store.load();

    cout << "\nWelcome, " << user << "! (" << store.size()
         << " saved artifact(s) loaded)\n";

    int choice;
    do
    {
        cout << "\n===== Artifact Menu =====\n"
             << "1. Add Artifact\n"
             << "2. Display All Artifacts\n"
             << "3. Search Artifact by ID\n"
             << "4. Track Artifact Movement\n"
             << "5. Logout\n";
        choice = readInt("Enter your choice: ", 1, 5);

        switch (choice)
        {
            case 1: addArtifact(store);    break;
            case 2: store.displayAll();    break;
            case 3: searchArtifact(store); break;
            case 4:
                g_store = &store;
                movementMenu(user.c_str());
                g_store = nullptr;
                break;
            case 5: cout << "Logged out.\n"; break;
        }
    } while (choice != 5);
}

// ---------- Hub ----------
int main()
{
    cout << "===== ArtiTrack: Artifact Management System =====\n";

    int choice;
    do
    {
        cout << "\n1. Register\n2. Login\n3. Exit\n";
        choice = readInt("Choice: ", 1, 3);

        switch (choice)
        {
            case 1:
                registerUser();
                break;
            case 2:
            {
                string user;
                if (loginUser(user)) artifactMenu(user);
                else cout << "\nInvalid username or password.\n";
                break;
            }
            case 3:
                cout << "Goodbye!\n";
                break;
        }
    } while (choice != 3);

    return 0;
}