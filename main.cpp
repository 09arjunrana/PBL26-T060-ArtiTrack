// main.cpp - the hub: the ONLY file with main(). Menus that route to every module.
//
//   m0  login + input helpers          (C++)
//   m1  ArtifactStore, persistence     (C++)
//   m2  Artifact class hierarchy       (C++)
//   m3  search / sort / network        (C++ layer over the pure-C m3_core.c)
//   m4  movement, undo, restoration    (pure C, reached through bridge.cpp)
#include <iomanip>
#include <iostream>
#include <sstream>

#include "bridge.h"
#include "m0.h"
#include "m1.h"
#include "m2.h"
#include "m3.h"
#include "m4.h"
using namespace std;

// ---------- Artifact entry ----------
void addArtifact(ArtifactStore& store)
{
    cout << "\n--- Add Artifact ---\n"
         << "1. Pottery\n2. Coin\n3. Ancient Coin\n4. Weapon\n5. Sculpture\n6. Manuscript\n";
    int type = readInt("Select type: ", 1, 6);

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
    else if (type == 4)
    {
        double rust = readDouble("Enter Rust Index (0.0 - 1.0): ", 0.0, 1.0);
        a = make_unique<Weapon>(id, name, age, cond, loc, rust);
    }
    else if (type == 5)
    {
        double crack = readDouble("Enter Crack Index (0.0 - 1.0): ", 0.0, 1.0);
        a = make_unique<Sculpture>(id, name, age, cond, loc, crack);
    }
    else if (type == 6)
    {
        double decay = readDouble("Enter Decay Index (0.0 - 1.0): ", 0.0, 1.0);
        a = make_unique<Manuscript>(id, name, age, cond, loc, decay);
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

    ostringstream summary;
    summary << *a;                                  // uses the overloaded operator<<
    if (store.add(std::move(a)))
        cout << "Artifact saved successfully: " << summary.str() << "\n";
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
    cout << "  Priority score: " << fixed2(a->calculatePriority()) << "\n";
    if (const Restorable* r = dynamic_cast<const Restorable*>(a))   // pottery and coins only
        cout << "  Advice: " << r->restorationAdvice() << "\n";
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
             << "4. Search, Sort & Reports\n"
             << "5. Movement & Restoration\n"
             << "6. Logout\n";
        choice = readInt("Enter your choice: ", 1, 6);

        switch (choice)
        {
            case 1: addArtifact(store);    break;
            case 2: store.displayAll();    break;
            case 3: searchArtifact(store); break;
            case 4: searchAnalyticsMenu(store.all()); break;
            case 5:
                bridge_setStore(&store);               // lets the C module reach this store
                movementMenu(user.c_str());
                bridge_setStore(nullptr);
                break;
            case 6: cout << "Logged out.\n"; break;
        }
    } while (choice != 6);                              // 6 = Logout (this used to be 5, which exited after a move)
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
