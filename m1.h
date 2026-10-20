// m1.h - ArtifactStore: owns every artifact and keeps artifacts.txt in sync
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "m2.h"
using namespace std;

class ArtifactStore {
private:
    string filename;
    vector<unique_ptr<Artifact>> items;

    bool saveAll() const;      // rewrite the whole file (temp file, then swap in)

public:
    explicit ArtifactStore(const string& file = "artifacts.txt");

    void load();                                   // read everything saved by earlier sessions
    bool exists(const string& id) const;
    const Artifact* find(const string& id) const;
    Artifact* findMutable(const string& id);

    bool add(unique_ptr<Artifact> a);              // saves to disk first, keeps it in memory only if that worked
    bool updateLocation(const string& id, const string& newLoc);   // memory AND disk, rolled back on failure

    size_t size() const { return items.size(); }
    const vector<unique_ptr<Artifact>>& all() const { return items; }   // read-only view
    void displayAll() const;
};
