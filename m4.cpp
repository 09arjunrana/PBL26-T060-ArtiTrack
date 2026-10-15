// m4.cpp - Module 3: Search, Sorting & Graph Analytics (NO main here; used by m1.cpp)
// Everything here is read-only over the artifacts that ArtifactStore owns: Module 3
// keeps non-owning pointers and never reorders, copies or deletes the store's items.
#pragma once
#include "m0.cpp"
#include "m2.cpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace std;

// ---------- Exceptions ----------
// Module 3's "this shouldn't happen" cases. They derive from runtime_error so what()
// carries a readable message, and runGuarded() (bottom of this file) catches all three
// at the Module 3 menu, so none of them can crash or exit the program.
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

// ---------- Formatting helper ----------
// Formats through a private stream so cout's own settings are never changed
// (m1's displayAll already leaves setprecision(2) on cout).
string fixed2(double v)
{
    ostringstream os;
    os << fixed << setprecision(2) << v;
    return os.str();
}

// ---------- Hash index (fast lookup by ID) ----------
// Problem: ArtifactStore::find scans every artifact, O(n) per lookup. A hash table
// jumps straight to one short bucket, O(1) on average, so "find #4471" stays instant
// at thousands of artifacts. Separate chaining (a linked list per bucket) handles
// collisions simply and never "fills up" the way open addressing can.
class ArtifactHashIndex {
private:
    static constexpr size_t INITIAL_BUCKETS = 17;
    static constexpr double MAX_LOAD_FACTOR = 0.75;   // average chain length we allow

    vector<list<const Artifact*>> buckets;   // non-owning: ArtifactStore owns the artifacts
    size_t count = 0;
    int rehashCount = 0;

    // djb2 string hash (Dan Bernstein): h = h * 33 + c. Cheap, and the multiply
    // spreads near-identical IDs such as P001 / P002 into different buckets.
    static size_t hashString(const string& s)
    {
        size_t h = 5381;
        for (unsigned char c : s) h = h * 33 + c;   // unsigned overflow just wraps, fine for hashing
        return h;
    }

    size_t bucketOf(const string& id) const { return hashString(id) % buckets.size(); }

    // Grow to 2n+1 buckets and re-place every entry: an entry's bucket depends on
    // buckets.size(), so all of them must be recomputed. Odd sizes (not powers of two)
    // make the modulo use the whole hash value, not just its lowest bits.
    void rehash()
    {
        vector<list<const Artifact*>> bigger(buckets.size() * 2 + 1);
        for (const auto& chain : buckets)
            for (const Artifact* a : chain)
                bigger[hashString(a->getId()) % bigger.size()].push_back(a);
        buckets.swap(bigger);
        rehashCount++;
    }

public:
    ArtifactHashIndex() : buckets(INITIAL_BUCKETS) {}

    void insert(const Artifact* a)
    {
        list<const Artifact*>& chain = buckets[bucketOf(a->getId())];
        for (const Artifact* existing : chain)
            if (existing->getId() == a->getId()) throw DuplicateIdException(a->getId());
        chain.push_back(a);
        count++;
        if (loadFactor() > MAX_LOAD_FACTOR) rehash();
    }

    const Artifact* find(const string& id) const
    {
        for (const Artifact* a : buckets[bucketOf(id)])
            if (a->getId() == id) return a;
        throw ArtifactNotFoundException(id);
    }

    // Called every time Module 3 opens, so the index always matches what the store holds
    void rebuild(const vector<unique_ptr<Artifact>>& items)
    {
        buckets.assign(INITIAL_BUCKETS, list<const Artifact*>());
        count = 0;
        rehashCount = 0;
        for (const auto& a : items) insert(a.get());
    }

    size_t size() const { return count; }
    size_t bucketCount() const { return buckets.size(); }
    double loadFactor() const { return double(count) / buckets.size(); }
    int rehashes() const { return rehashCount; }

    size_t longestChain() const
    {
        size_t longest = 0;
        for (const auto& chain : buckets) longest = max(longest, chain.size());
        return longest;
    }
};

// ---------- Merge sort (ordering tools) ----------
// Problem: reports need artifacts in age / condition / priority order. Merge sort is
// O(n log n) in every case and stable: artifacts with equal keys keep their original
// order, so ties in a sorted report still appear in save-file order.
// Trade-off: it needs an O(n) buffer. Quicksort sorts in place but is not stable and
// degrades to O(n^2) on bad pivots; std::sort (introsort) is fast but not stable, and
// would hide the algorithm this module has to demonstrate.
template <typename T, typename Less>
void mergeSortRange(vector<T>& v, vector<T>& buffer, size_t lo, size_t hi, const Less& less)
{
    if (hi - lo < 2) return;                 // 0 or 1 element: already sorted
    size_t mid = lo + (hi - lo) / 2;
    mergeSortRange(v, buffer, lo, mid, less);
    mergeSortRange(v, buffer, mid, hi, less);

    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi)
    {
        // Take from the right half only when it is strictly smaller. On a tie the left
        // element (the one that came first) goes first: this is what makes it stable.
        if (less(v[j], v[i])) buffer[k++] = v[j++];
        else                  buffer[k++] = v[i++];
    }
    while (i < mid) buffer[k++] = v[i++];
    while (j < hi)  buffer[k++] = v[j++];
    for (k = lo; k < hi; k++) v[k] = buffer[k];
}

// One generic routine for every sort in Module 3; the comparator picks key and direction
template <typename T, typename Less>
void mergeSort(vector<T>& v, const Less& less)
{
    vector<T> buffer(v.size());
    mergeSortRange(v, buffer, 0, v.size(), less);
}

// ---------- Era bands ----------
// Eras derived from estimatedAge (years before today). The cut-offs are our own rough
// approximations of common historical periods, counted back from 2026.
const int EARLY_MODERN_MIN_AGE = 200;    // made around 1826 or earlier
const int MEDIEVAL_MIN_AGE     = 500;    // around 1526 or earlier
const int ANCIENT_MIN_AGE      = 1500;   // around 526 CE or earlier

enum class Era { Modern, EarlyModern, Medieval, Ancient };

Era eraOf(int age)
{
    if (age >= ANCIENT_MIN_AGE)      return Era::Ancient;
    if (age >= MEDIEVAL_MIN_AGE)     return Era::Medieval;
    if (age >= EARLY_MODERN_MIN_AGE) return Era::EarlyModern;
    return Era::Modern;
}

string eraName(Era e)
{
    if (e == Era::Ancient)     return "Ancient";
    if (e == Era::Medieval)    return "Medieval";
    if (e == Era::EarlyModern) return "Early Modern";
    return "Modern";
}

// ---------- Risk bands ----------
// From m2.cpp, the condition part of the base formula, (10 - condition) * 4, runs from
// 0 to 40. Age adds 1.5 per century on top, and type bonuses add up to +50 (pottery).
// The full range (0 to ~1590 at age 100000) is dominated by age, so splitting it into
// thirds would call almost everything Low. Instead each band is the score of a generic,
// age-0 artifact at a given condition:
const double RISK_MEDIUM_MIN = 20.0;   // = (10 - 5) * 4 : as urgent as a half-ruined artifact
const double RISK_HIGH_MIN   = 40.0;   // = (10 - 0) * 4 : as urgent as a fully ruined artifact

// A storage location is flagged when its *average* artifact is already High risk
const double RISKY_LOCATION_AVG_PRIORITY = RISK_HIGH_MIN;

// ---------- Report rows (friend of Artifact) ----------
// One fixed-width row per artifact. It is a friend of Artifact (declared in m2.cpp), so
// the report layout lives here in Module 3 and reads the fields directly, instead of
// adding another formatting method to the shared class.
void printReportRow(ostream& out, const Artifact& a)
{
    string shortName = a.name.size() > 20 ? a.name.substr(0, 19) + "~" : a.name;
    out << "  " << left << setw(8) << a.id << ' ' << setw(20) << shortName << ' '
        << setw(12) << a.typeName() << right
        << setw(7) << a.estimatedAge << setw(7) << fixed2(a.conditionScore)
        << setw(10) << fixed2(a.calculatePriority()) << "  " << a.currentLocation << "\n";
}

void printReportHeader(ostream& out)
{
    out << "  " << left << setw(8) << "ID" << ' ' << setw(20) << "Name" << ' '
        << setw(12) << "Type" << right
        << setw(7) << "Age" << setw(7) << "Cond" << setw(10) << "Priority" << "  Location\n";
}

using ArtifactView = vector<const Artifact*>;   // non-owning list of artifacts, in some order

void printArtifactTable(const string& title, const ArtifactView& rows)
{
    cout << "\n=== " << title << " (" << rows.size() << ") ===\n";
    if (rows.empty())
    {
        cout << "  (none)\n";
        return;
    }
    printReportHeader(cout);
    for (const Artifact* a : rows) printReportRow(cout, *a);
}

// ---------- Search & analytics ----------
enum class SortKey { Age, Condition, Priority };

// Per-location totals for the risky storage report
struct LocationStats {
    string location;
    int count = 0;
    int highRisk = 0;
    double totalCondition = 0.0;
    double totalPriority = 0.0;

    double avgCondition() const { return totalCondition / count; }
    double avgPriority() const { return totalPriority / count; }
};

// Single responsibility: answer read-only questions about the loaded collection
// (find, filter, sort, report). It owns only its hash index; the artifacts stay owned
// by ArtifactStore, and every result is an ArtifactView of pointers into the store.
class SearchAnalytics {
private:
    const vector<unique_ptr<Artifact>>& items;
    ArtifactHashIndex index;

    // The artifacts that pass a test, in store order
    template <typename Keep>
    ArtifactView filtered(const Keep& keep) const
    {
        ArtifactView out;
        for (const auto& a : items)
            if (keep(*a)) out.push_back(a.get());
        return out;
    }

public:
    explicit SearchAnalytics(const vector<unique_ptr<Artifact>>& store) : items(store)
    {
        index.rebuild(items);
    }

    const ArtifactHashIndex& hashIndex() const { return index; }

    // Throws ArtifactNotFoundException on a miss
    const Artifact& findById(const string& id) const { return *index.find(id); }

    ArtifactView all() const
    {
        return filtered([](const Artifact&) { return true; });
    }

    ArtifactView sortedBy(SortKey key, bool descending) const
    {
        ArtifactView v = all();
        auto keyOf = [key](const Artifact* a) -> double {
            if (key == SortKey::Age)       return a->getAge();
            if (key == SortKey::Condition) return a->getCondition();
            return a->calculatePriority();
        };
        if (descending)
            mergeSort(v, [&](const Artifact* x, const Artifact* y) { return keyOf(x) > keyOf(y); });
        else
            mergeSort(v, [&](const Artifact* x, const Artifact* y) { return keyOf(x) < keyOf(y); });
        return v;
    }

    // Exact type: "Coin" does not include ancient coins, which have their own type
    ArtifactView byType(const string& type) const
    {
        return filtered([&](const Artifact& a) { return a.typeName() == type; });
    }

    ArtifactView byEra(Era era) const
    {
        return filtered([&](const Artifact& a) { return eraOf(a.getAge()) == era; });
    }

    // Inclusive at both ends
    ArtifactView byConditionRange(double lo, double hi) const
    {
        return filtered([&](const Artifact& a) {
            return a.getCondition() >= lo && a.getCondition() <= hi;
        });
    }

    void urgentCareReport() const
    {
        ArtifactView high, medium, low;
        for (const Artifact* a : sortedBy(SortKey::Priority, true))   // highest first
        {
            double p = a->calculatePriority();
            if (p >= RISK_HIGH_MIN)        high.push_back(a);
            else if (p >= RISK_MEDIUM_MIN) medium.push_back(a);
            else                           low.push_back(a);
        }

        cout << "\n===== Urgent Care Report =====\n"
             << "Low / medium / high risk banding, as in the English Heritage risk & condition\n"
             << "audit from our literature survey. The thresholds are ours, from m2.cpp's formulas:\n"
             << "  High   : priority >= " << fixed2(RISK_HIGH_MIN)
             << "  (as urgent as a fully ruined artifact, condition 0)\n"
             << "  Medium : priority >= " << fixed2(RISK_MEDIUM_MIN)
             << "  (as urgent as a half-ruined artifact, condition 5)\n"
             << "  Low    : priority <  " << fixed2(RISK_MEDIUM_MIN) << "\n"
             << "  Age adds 1.5 points per century, so older artifacts move up the bands.\n";
        printArtifactTable("HIGH RISK - restore first", high);
        printArtifactTable("MEDIUM RISK", medium);
        printArtifactTable("LOW RISK", low);
    }

    void riskyStorageReport() const
    {
        cout << "\n===== Risky Storage Report =====\n";
        if (items.empty())
        {
            cout << "No artifacts loaded.\n";
            return;
        }

        map<string, LocationStats> groups;   // one entry per distinct currentLocation
        for (const auto& a : items)
        {
            LocationStats& s = groups[a->getLocation()];
            double p = a->calculatePriority();
            s.location = a->getLocation();
            s.count++;
            s.totalCondition += a->getCondition();
            s.totalPriority += p;
            if (p >= RISK_HIGH_MIN) s.highRisk++;
        }

        vector<LocationStats> rows;
        for (const auto& entry : groups) rows.push_back(entry.second);
        mergeSort(rows, [](const LocationStats& x, const LocationStats& y) {
            return x.avgPriority() > y.avgPriority();   // riskiest location first
        });

        cout << "Flag (!!) = average priority >= " << fixed2(RISKY_LOCATION_AVG_PRIORITY)
             << " (the average artifact stored there is High risk)\n\n"
             << "     " << left << setw(22) << "Location" << right << setw(7) << "Items"
             << setw(10) << "Avg cond" << setw(14) << "Avg priority" << setw(11) << "High risk" << "\n";
        for (const LocationStats& s : rows)
        {
            bool risky = s.avgPriority() >= RISKY_LOCATION_AVG_PRIORITY;
            cout << "  " << (risky ? "!!" : "  ") << " " << left << setw(22) << s.location << right
                 << setw(7) << s.count << setw(10) << fixed2(s.avgCondition())
                 << setw(14) << fixed2(s.avgPriority()) << setw(11) << s.highRisk << "\n";
        }
    }
};

// ---------- Movement frequency (waits on Module 2) ----------
// The movement log is not built yet, so this takes ready-made counts
// (artifact ID -> number of moves) instead of guessing at a file format.
void movementFrequencyReport(const map<string, int>& movesPerArtifact)
{
    cout << "\n===== Movement Frequency Report =====\n";
    if (movesPerArtifact.empty())
    {
        cout << "No movements recorded.\n";
        return;
    }

    vector<pair<string, int>> rows(movesPerArtifact.begin(), movesPerArtifact.end());
    // Most-moved first; stability keeps tied artifacts in ID order (the map's order)
    mergeSort(rows, [](const pair<string, int>& x, const pair<string, int>& y) {
        return x.second > y.second;
    });

    cout << "  " << left << setw(10) << "ID" << right << setw(7) << "Moves" << "\n";
    for (const auto& r : rows)
        cout << "  " << left << setw(10) << r.first << right << setw(7) << r.second << "\n";
}

// ---------- Location network (graph of sites) ----------
// Problem: artifacts move between many sites, and staff need to know how the sites
// connect. An undirected graph as an adjacency list costs O(V + E) memory and lists a
// site's neighbours directly; an adjacency matrix would be O(V^2) and mostly empty for
// a sparse set of routes. std::map keeps sites sorted, so output is the same every run.
class LocationNetwork {
private:
    string filename;
    map<string, vector<string>> adj;   // site -> directly connected sites

    bool connected(const string& a, const string& b) const
    {
        auto it = adj.find(a);
        if (it == adj.end()) return false;
        for (const string& n : it->second)
            if (n == b) return true;
        return false;
    }

    // Recursive DFS: visit a site, then each unvisited neighbour, going as deep as possible
    void dfs(const string& site, set<string>& seen, vector<string>& order) const
    {
        seen.insert(site);
        order.push_back(site);
        for (const string& next : adj.at(site))
            if (!seen.count(next)) dfs(next, seen, order);
    }

public:
    explicit LocationNetwork(const string& file = "network.txt") : filename(file) {}

    void addSite(const string& site) { adj[site]; }   // operator[] creates an empty neighbour list

    void addSitesFrom(const vector<unique_ptr<Artifact>>& items)
    {
        for (const auto& a : items) addSite(a->getLocation());
    }

    bool hasSite(const string& site) const { return adj.count(site) > 0; }
    size_t siteCount() const { return adj.size(); }

    // In memory only. Undirected, so the edge goes into both sites' lists.
    bool connect(const string& a, const string& b)
    {
        if (a == b || connected(a, b)) return false;
        adj[a].push_back(b);
        adj[b].push_back(a);
        return true;
    }

    // Read network.txt (one SITE_A|SITE_B per line). A missing file is normal on the
    // first run; a file that exists but cannot be read throws FileAccessException.
    void load()
    {
        error_code ec;   // makes the filesystem checks report failure instead of throwing
        if (!filesystem::exists(filename, ec)) return;
        if (!filesystem::is_regular_file(filename, ec)) throw FileAccessException(filename, "read");

        ifstream in(filename);
        if (!in.is_open()) throw FileAccessException(filename, "read");

        string line;
        int skipped = 0;
        while (getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();   // file saved with Windows line endings
            if (line.empty()) continue;
            size_t bar = line.find('|');
            if (bar == string::npos || bar == 0 || bar == line.size() - 1 ||
                line.find('|', bar + 1) != string::npos)
            {
                skipped++;
                continue;
            }
            connect(line.substr(0, bar), line.substr(bar + 1));
        }
        if (in.bad()) throw FileAccessException(filename, "read");
        if (skipped > 0)
            cout << "Warning: skipped " << skipped << " malformed line(s) in " << filename << ".\n";
    }

    // Saves to disk first; only adds the connection in memory if the save worked.
    // Returns false (and writes nothing) for a self-loop or an existing connection.
    bool addConnection(const string& a, const string& b)
    {
        if (a == b || connected(a, b)) return false;
        ofstream out(filename, ios::app);
        if (!out.is_open()) throw FileAccessException(filename, "write");
        out << a << "|" << b << "\n";
        out.flush();
        if (!out) throw FileAccessException(filename, "write");
        return connect(a, b);
    }

    // TODO(integration): Module 2 -- pass (from, to) location pairs from the movement
    // records here so every real transfer becomes a route. Kept in memory only, because
    // the movement log already saves those transfers.
    void addEdgesFromMovements(const vector<pair<string, string>>& transfers)
    {
        for (const auto& t : transfers) connect(t.first, t.second);
    }

    // BFS explores sites in rings of 1, 2, 3... transfers, so the first time it reaches
    // `to` is along a fewest-transfers route. parent[] records how each site was reached.
    // Returns the route from `from` to `to`, or an empty vector if there is none.
    vector<string> shortestPath(const string& from, const string& to) const
    {
        if (!hasSite(from) || !hasSite(to)) return {};

        map<string, string> parent;
        queue<string> pending;
        parent[from] = from;
        pending.push(from);
        while (!pending.empty())
        {
            string cur = pending.front();
            pending.pop();
            if (cur == to) break;
            for (const string& next : adj.at(cur))
            {
                if (parent.count(next)) continue;   // already reached by a route no longer than this
                parent[next] = cur;
                pending.push(next);
            }
        }
        if (!parent.count(to)) return {};   // `to` is in a different cluster

        vector<string> path;
        for (string s = to; s != from; s = parent[s]) path.push_back(s);
        path.push_back(from);
        reverse(path.begin(), path.end());
        return path;
    }

    // Every site reachable from `site`, in DFS visiting order
    vector<string> reachableFrom(const string& site) const
    {
        vector<string> order;
        if (!hasSite(site)) return order;
        set<string> seen;
        dfs(site, seen, order);
        return order;
    }

    // Each DFS started from a not-yet-seen site uncovers one whole cluster
    int clusterCount() const
    {
        set<string> seen;
        vector<string> unused;
        int clusters = 0;
        for (const auto& entry : adj)
        {
            if (seen.count(entry.first)) continue;
            dfs(entry.first, seen, unused);
            clusters++;
        }
        return clusters;
    }

    void print() const
    {
        cout << "\n=== " << adj.size() << " site(s) ===\n";
        for (const auto& entry : adj)
        {
            cout << "  " << entry.first << " -> ";
            if (entry.second.empty()) cout << "(no connections)";
            for (size_t i = 0; i < entry.second.size(); i++)
                cout << (i > 0 ? ", " : "") << entry.second[i];
            cout << "\n";
        }
    }
};

// ---------- Module 3 submenus ----------
void findByIdMenu(const SearchAnalytics& sa)
{
    string id = readLine("Enter Artifact ID: ");
    const Artifact& a = sa.findById(id);   // throws ArtifactNotFoundException on a miss
    a.display();
    cout << "  Era: " << eraName(eraOf(a.getAge()))
         << " | Priority score: " << fixed2(a.calculatePriority()) << "\n";
}

void filteredSearchMenu(const SearchAnalytics& sa)
{
    cout << "\n--- Filtered Search ---\n"
         << "1. By type\n2. By era\n3. By condition range\n";
    int how = readInt("Select filter: ", 1, 3);

    if (how == 1)
    {
        cout << "1. Artifact (generic)\n2. Pottery\n3. Coin\n4. Ancient Coin\n";
        static const char* types[] = {"Artifact", "Pottery", "Coin", "Ancient Coin"};
        string type = types[readInt("Select type: ", 1, 4) - 1];
        printArtifactTable("Type: " + type, sa.byType(type));
    }
    else if (how == 2)
    {
        cout << "1. Modern        (age under " << EARLY_MODERN_MIN_AGE << ")\n"
             << "2. Early Modern  (age " << EARLY_MODERN_MIN_AGE << " - " << MEDIEVAL_MIN_AGE - 1 << ")\n"
             << "3. Medieval      (age " << MEDIEVAL_MIN_AGE << " - " << ANCIENT_MIN_AGE - 1 << ")\n"
             << "4. Ancient       (age " << ANCIENT_MIN_AGE << " and over)\n";
        static const Era eras[] = {Era::Modern, Era::EarlyModern, Era::Medieval, Era::Ancient};
        Era era = eras[readInt("Select era: ", 1, 4) - 1];
        printArtifactTable("Era: " + eraName(era), sa.byEra(era));
    }
    else
    {
        double lo = readDouble("Minimum condition (0 - 10): ", 0.0, 10.0);
        double hi = readDouble("Maximum condition (" + fixed2(lo) + " - 10): ", lo, 10.0);
        printArtifactTable("Condition " + fixed2(lo) + " to " + fixed2(hi),
                           sa.byConditionRange(lo, hi));
    }
}

void sortedListingMenu(const SearchAnalytics& sa)
{
    cout << "\n--- Sorted Listing ---\n"
         << "1. Age\n2. Condition\n3. Priority\n";
    int key = readInt("Sort by: ", 1, 3);
    cout << "1. Ascending\n2. Descending\n";
    bool descending = readInt("Order: ", 1, 2) == 2;

    static const SortKey keys[] = {SortKey::Age, SortKey::Condition, SortKey::Priority};
    static const char* keyNames[] = {"age", "condition", "priority"};
    printArtifactTable(string("Sorted by ") + keyNames[key - 1] +
                           (descending ? ", descending" : ", ascending"),
                       sa.sortedBy(keys[key - 1], descending));
}

void reportsMenu(const SearchAnalytics& sa)
{
    cout << "\n--- Reports ---\n"
         << "1. Urgent care (risk bands)\n"
         << "2. Risky storage (by location)\n"
         << "3. Movement frequency\n";
    int report = readInt("Select report: ", 1, 3);

    if (report == 1) sa.urgentCareReport();
    else if (report == 2) sa.riskyStorageReport();
    else
    {
        // TODO(integration): Module 2 -- count moves per artifact ID from the movement
        // log and call movementFrequencyReport(counts) here.
        cout << "Movement data not available yet (waiting on Module 2: Movement tracking).\n";
    }
}

void printRoute(const vector<string>& path)
{
    for (size_t i = 0; i < path.size(); i++)
        cout << (i > 0 ? " -> " : "  ") << path[i];
    cout << "\n";
}

// Sites come from the artifacts' locations plus the connections saved in network.txt
void networkMenu(const vector<unique_ptr<Artifact>>& items)
{
    LocationNetwork net;
    net.addSitesFrom(items);
    net.load();   // throws FileAccessException if network.txt exists but cannot be read

    int choice;
    do
    {
        cout << "\n--- Location Network ---\n"
             << "1. Show sites and connections\n"
             << "2. Add connection\n"
             << "3. Fewest transfers between two sites (BFS)\n"
             << "4. Sites reachable from a site + clusters (DFS)\n"
             << "5. Back\n";
        choice = readInt("Enter your choice: ", 1, 5);

        switch (choice)
        {
            case 1:
                net.print();
                break;
            case 2:
            {
                string a = readLine("First site: ");
                string b = readLine("Second site: ");
                if (net.addConnection(a, b)) cout << "Connection saved.\n";
                else cout << "Not added: a site cannot connect to itself, or this connection already exists.\n";
                break;
            }
            case 3:
            {
                string from = readLine("From site: ");
                string to   = readLine("To site: ");
                if (!net.hasSite(from) || !net.hasSite(to))
                {
                    cout << "Unknown site. Use option 1 to see the site names.\n";
                    break;
                }
                vector<string> path = net.shortestPath(from, to);
                if (path.empty())
                {
                    cout << "No route: these sites are in different clusters.\n";
                    break;
                }
                cout << "Fewest transfers: " << path.size() - 1 << "\n";
                printRoute(path);
                break;
            }
            case 4:
            {
                string site = readLine("Start site: ");
                if (!net.hasSite(site))
                {
                    cout << "Unknown site. Use option 1 to see the site names.\n";
                    break;
                }
                vector<string> reach = net.reachableFrom(site);
                cout << reach.size() << " site(s) reachable from " << site << " (DFS order):\n";
                printRoute(reach);
                cout << "The network has " << net.clusterCount() << " separate cluster(s) of "
                     << net.siteCount() << " site(s).\n";
                break;
            }
            case 5:
                break;
        }
    } while (choice != 5);
}

// ---------- Module 3 boundary ----------
// Every Module 3 exception stops here with a clear message, so a bad ID or file never
// crashes the program or throws the user out of the menu.
template <typename Action>
void runGuarded(const Action& action)
{
    try
    {
        action();
    }
    catch (const ArtifactNotFoundException& e) { cout << "Not found: " << e.what() << ".\n"; }
    catch (const DuplicateIdException& e)      { cout << "Data error: " << e.what() << ".\n"; }
    catch (const FileAccessException& e)       { cout << "File error: " << e.what() << ".\n"; }
}

// ---------- Module 3 entry point (from the artifact menu) ----------
// Takes the store's items rather than ArtifactStore itself, so m4.cpp never needs
// m1.cpp (m1 includes m4, never the other way round).
void searchAnalyticsMenu(const vector<unique_ptr<Artifact>>& items)
{
    // The tables use setw/left/right; save cout's settings so the artifact menu
    // prints exactly as before once the user goes Back.
    ios_base::fmtflags savedFlags = cout.flags();
    streamsize savedPrecision = cout.precision();
    cout << defaultfloat << setprecision(6);

    unique_ptr<SearchAnalytics> sa;
    runGuarded([&] { sa = make_unique<SearchAnalytics>(items); });   // builds the hash index

    if (sa)
    {
        const ArtifactHashIndex& idx = sa->hashIndex();
        cout << "\nHash index ready: " << idx.size() << " artifact(s) in " << idx.bucketCount()
             << " buckets (load factor " << fixed2(idx.loadFactor()) << ", "
             << idx.rehashes() << " rehash(es)).\n";

        int choice;
        do
        {
            cout << "\n===== Search, Sort & Reports =====\n"
                 << "1. Find by ID (hash)\n"
                 << "2. Filtered search\n"
                 << "3. Sorted listing\n"
                 << "4. Reports\n"
                 << "5. Location network\n"
                 << "6. Back\n";
            choice = readInt("Enter your choice: ", 1, 6);

            runGuarded([&] {
                switch (choice)
                {
                    case 1: findByIdMenu(*sa);       break;
                    case 2: filteredSearchMenu(*sa); break;
                    case 3: sortedListingMenu(*sa);  break;
                    case 4: reportsMenu(*sa);        break;
                    case 5: networkMenu(items);      break;
                    case 6: break;
                }
            });
        } while (choice != 6);
    }

    cout.flags(savedFlags);
    cout.precision(savedPrecision);
}
