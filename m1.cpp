// m1.cpp - Module 1: Artifact persistence (ArtifactStore). NO main here; see main.cpp.
#include "m1.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_set>
using namespace std;

ArtifactStore::ArtifactStore(const string& file) : filename(file) {}

// Rewrites the whole save file (write to a temp file, then swap it in)
bool ArtifactStore::saveAll() const
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

void ArtifactStore::load()
{
    items.clear();
    ifstream in(filename);
    if (!in.is_open()) return;          // first run: nothing saved yet

    unordered_set<string> seen;         // O(1) duplicate check while loading
    string line;
    int skipped = 0;
    while (getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // file saved with Windows line endings
        if (line.empty()) continue;
        unique_ptr<Artifact> a = parseArtifact(line);
        if (a && seen.insert(a->getId()).second) items.push_back(std::move(a));
        else skipped++;
    }
    if (skipped > 0)
    {
        cout << "Warning: skipped " << skipped
             << " unreadable or duplicate line(s) in " << filename << ".\n";
        // Later saves rewrite the whole file, which would erase those lines,
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

bool ArtifactStore::exists(const string& id) const { return find(id) != nullptr; }

const Artifact* ArtifactStore::find(const string& id) const
{
    for (const auto& a : items)
        if (a->getId() == id) return a.get();
    return nullptr;
}

Artifact* ArtifactStore::findMutable(const string& id)
{
    for (auto& a : items)
        if (a->getId() == id) return a.get();
    return nullptr;
}

// Saves to disk first; only keeps the artifact in memory if the save worked
bool ArtifactStore::add(unique_ptr<Artifact> a)
{
    ofstream out(filename, ios::app);
    if (!out.is_open()) return false;
    out << a->serialize() << "\n";
    out.flush();
    if (!out) return false;
    items.push_back(std::move(a));
    return true;
}

// Changes an artifact's location in memory AND on disk.
// If the save fails the old location is restored and false is returned.
bool ArtifactStore::updateLocation(const string& id, const string& newLoc)
{
    Artifact* a = findMutable(id);
    if (!a) return false;
    string oldLoc = a->getLocation();
    a->setLocation(newLoc);
    if (saveAll()) return true;
    a->setLocation(oldLoc);
    return false;
}

void ArtifactStore::displayAll() const
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
