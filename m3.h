// m3.h - Module 3: Search, Sorting & Graph Analytics (C++ layer over the pure-C core in m3_core.c)
//
// Division of labour:
//   m3_core.c / m3_core.h  PURE C   hash table, stable merge sort, graph (BFS / DFS)
//   m3.cpp / m3.h          C++      wraps the C code in classes, adds exceptions, filters,
//                                   reports and the menus. Read-only over the artifacts.
#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "m2.h"
#include "m3_core.h"
using namespace std;

// ---------- Exceptions (all derive from runtime_error so what() is readable) ----------
class ArtifactNotFoundException : public runtime_error {
public:
    explicit ArtifactNotFoundException(const string& id)
        : runtime_error("no artifact has the ID '" + id + "'") {}
};

class DuplicateIdException : public runtime_error {
public:
    explicit DuplicateIdException(const string& id)
        : runtime_error("the ID '" + id + "' appears more than once in the data") {}
};

class FileAccessException : public runtime_error {
public:
    FileAccessException(const string& file, const string& action)
        : runtime_error("could not " + action + " the file '" + file + "'") {}
};

string fixed2(double v);                       // 2 decimals, without touching cout's own settings
vector<string> splitFields(const string& line);

// ---------- Stable merge sort for any vector ----------
// Sorts an array of INDEXES with the C merge_sort, then rearranges the vector. (merge_sort
// copies raw bytes, which is not safe for std::string, so it never touches the elements.)
// `less(x, y)` must be a strict "x goes before y" test.
template <typename T, typename Less>
struct SortContext {
    const vector<T>* data;
    const Less* less;
};

template <typename T, typename Less>
int compareIndexes(const void* a, const void* b, void* ctx)
{
    const SortContext<T, Less>* c = static_cast<const SortContext<T, Less>*>(ctx);
    const T& x = (*c->data)[*static_cast<const size_t*>(a)];
    const T& y = (*c->data)[*static_cast<const size_t*>(b)];
    if ((*c->less)(x, y)) return -1;
    if ((*c->less)(y, x)) return 1;
    return 0;
}

template <typename T, typename Less>
void mergeSort(vector<T>& v, const Less& less)
{
    vector<size_t> order(v.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;

    SortContext<T, Less> ctx{&v, &less};
    if (merge_sort(order.data(), order.size(), sizeof(size_t), compareIndexes<T, Less>, &ctx) != 0)
        throw bad_alloc();

    vector<T> sorted;
    sorted.reserve(v.size());
    for (size_t i : order) sorted.push_back(v[i]);
    v.swap(sorted);
}

// ---------- Hash index (wraps the C HashIndex) ----------
class ArtifactHashIndex {
private:
    HashIndex table;
public:
    ArtifactHashIndex();
    ~ArtifactHashIndex();
    ArtifactHashIndex(const ArtifactHashIndex&) = delete;
    ArtifactHashIndex& operator=(const ArtifactHashIndex&) = delete;

    void insert(const Artifact* a);                         // throws DuplicateIdException
    const Artifact* find(const string& id) const;           // throws ArtifactNotFoundException
    void rebuild(const vector<unique_ptr<Artifact>>& items);

    size_t size() const { return table.count; }
    size_t bucketCount() const { return table.bucketCount; }
    double loadFactor() const { return hi_loadFactor(&table); }
    int rehashes() const { return table.rehashes; }
    size_t longestChain() const { return hi_longestChain(&table); }
};

// ---------- Era and risk bands ----------
const int EARLY_MODERN_MIN_AGE = 200;
const int MEDIEVAL_MIN_AGE     = 500;
const int ANCIENT_MIN_AGE      = 1500;
enum class Era { Modern, EarlyModern, Medieval, Ancient };
Era eraOf(int age);
string eraName(Era e);

const double RISK_MEDIUM_MIN = 20.0;
const double RISK_HIGH_MIN   = 40.0;
const double RISKY_LOCATION_AVG_PRIORITY = RISK_HIGH_MIN;

// ---------- Report helpers ----------
using ArtifactView = vector<const Artifact*>;      // non-owning list of artifacts, in some order
void printReportRow(ostream& out, const Artifact& a);
void printReportHeader(ostream& out);
void printArtifactTable(const string& title, const ArtifactView& rows);

enum class SortKey { Age, Condition, Priority };

struct LocationStats {
    string location;
    int count = 0;
    int highRisk = 0;
    double totalCondition = 0.0;
    double totalPriority = 0.0;
    double avgCondition() const { return totalCondition / count; }
    double avgPriority() const { return totalPriority / count; }
};

class SearchAnalytics {
private:
    const vector<unique_ptr<Artifact>>& items;
    ArtifactHashIndex index;

    template <typename Keep>
    ArtifactView filtered(const Keep& keep) const
    {
        ArtifactView out;
        for (const auto& a : items)
            if (keep(*a)) out.push_back(a.get());
        return out;
    }

public:
    explicit SearchAnalytics(const vector<unique_ptr<Artifact>>& store);

    const ArtifactHashIndex& hashIndex() const { return index; }
    const Artifact& findById(const string& id) const;     // throws ArtifactNotFoundException
    ArtifactView all() const;
    ArtifactView sortedBy(SortKey key, bool descending) const;
    ArtifactView byType(const string& type) const;
    ArtifactView byEra(Era era) const;
    ArtifactView byConditionRange(double lo, double hi) const;
    void urgentCareReport() const;
    void riskyStorageReport() const;
};

// ---------- Movement log (read-only; written by m4.c) ----------
// One move per line:  MOVE|ARTIFACT_ID|FROM|TO|USER|REASON|TIMESTAMP   (7 fields)
extern const char* MOVEMENT_FILE;
const size_t MOVEMENT_FIELDS = 7;

struct MovementRecord {
    string artifactId, from, to, movedBy, reason, timestamp;
};

vector<MovementRecord> loadMovements(const string& file = MOVEMENT_FILE);
map<string, int> countMovesPerArtifact(const vector<MovementRecord>& moves);
void movementFrequencyReport(const map<string, int>& movesPerArtifact);

// ---------- Location network (wraps the C Graph) ----------
class LocationNetwork {
private:
    string filename;
    Graph* graph;

public:
    explicit LocationNetwork(const string& file = "network.txt");
    ~LocationNetwork();
    LocationNetwork(const LocationNetwork&) = delete;
    LocationNetwork& operator=(const LocationNetwork&) = delete;

    void addSite(const string& site);
    void addSitesFrom(const vector<unique_ptr<Artifact>>& items);
    bool hasSite(const string& site) const;
    size_t siteCount() const;
    bool connect(const string& a, const string& b);              // in memory only
    void load();                                                 // network.txt; throws FileAccessException
    bool addConnection(const string& a, const string& b);        // saves first; throws FileAccessException
    void addEdgesFromMovements(const vector<MovementRecord>& moves);
    vector<string> shortestPath(const string& from, const string& to) const;   // BFS
    vector<string> reachableFrom(const string& site) const;                    // DFS
    int clusterCount() const;
    void print() const;
};

// ---------- Menu entry point (called from the artifact menu) ----------
void searchAnalyticsMenu(const vector<unique_ptr<Artifact>>& items);
