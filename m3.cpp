// m3.cpp - Module 3: Search, Sorting & Graph Analytics, C++ layer (NO main here)
//
// The data structures themselves (hash table, merge sort, BFS/DFS graph) are written in
// PURE C in m3_core.c. This file wraps them in classes, turns their C return codes into
// exceptions, and builds the filters, reports and menus on top.
// Everything here is read-only over the artifacts ArtifactStore owns: Module 3 keeps
// non-owning pointers and never reorders, copies or deletes the store's items.
#include "m3.h"
#include "m0.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
using namespace std;

// ---------- Formatting and file helpers ----------
// Formats through a private stream so cout's own settings are never changed
string fixed2(double v)
{
    ostringstream os;
    os << fixed << setprecision(2) << v;
    return os.str();
}

// Shared by network.txt and movements.txt. A missing file is normal (nothing saved
// yet) and returns false; a file that exists but cannot be read throws.
static bool openDataFile(const string& file, ifstream& in)
{
    error_code ec;   // makes the filesystem checks report failure instead of throwing
    if (!filesystem::exists(file, ec)) return false;
    if (!filesystem::is_regular_file(file, ec)) throw FileAccessException(file, "read");
    in.open(file);
    if (!in.is_open()) throw FileAccessException(file, "read");
    return true;
}

// Split one line on '|', the field separator in every ArtiTrack save file
vector<string> splitFields(const string& line)
{
    vector<string> f;
    string cur;
    for (char c : line)
    {
        if (c == '|') { f.push_back(cur); cur.clear(); }
        else cur += c;
    }
    f.push_back(cur);
    return f;
}

// ---------- Hash index: C++ wrapper over the C HashIndex ----------
ArtifactHashIndex::ArtifactHashIndex()
{
    if (hi_init(&table) != 0) throw bad_alloc();
}

ArtifactHashIndex::~ArtifactHashIndex() { hi_free(&table); }

void ArtifactHashIndex::insert(const Artifact* a)
{
    int rc = hi_insert(&table, a->getId().c_str(), a);
    if (rc == HI_DUPLICATE) throw DuplicateIdException(a->getId());
    if (rc == HI_NOMEM) throw bad_alloc();
}

const Artifact* ArtifactHashIndex::find(const string& id) const
{
    const Artifact* a = static_cast<const Artifact*>(hi_find(&table, id.c_str()));
    if (a == nullptr) throw ArtifactNotFoundException(id);
    return a;
}

// Called every time Module 3 opens, so the index always matches what the store holds
void ArtifactHashIndex::rebuild(const vector<unique_ptr<Artifact>>& items)
{
    hi_free(&table);
    if (hi_init(&table) != 0) throw bad_alloc();
    for (const auto& a : items) insert(a.get());
}

// ---------- Era bands ----------
// Eras derived from estimatedAge (years before today). The cut-offs are our own rough
// approximations of common historical periods, counted back from 2026.
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


// ---------- Report rows (friend of Artifact) ----------
// One fixed-width row per artifact. It is a friend of Artifact (declared in m2.h), so
// the report layout lives here in Module 3 and reads the fields directly.
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
SearchAnalytics::SearchAnalytics(const vector<unique_ptr<Artifact>>& store) : items(store)
{
    index.rebuild(items);
}

// Throws ArtifactNotFoundException on a miss
const Artifact& SearchAnalytics::findById(const string& id) const { return *index.find(id); }

ArtifactView SearchAnalytics::all() const
{
    return filtered([](const Artifact&) { return true; });
}

// Priority uses the overloaded operator< from m2.h; age and condition use plain keys
ArtifactView SearchAnalytics::sortedBy(SortKey key, bool descending) const
{
    ArtifactView v = all();
    if (key == SortKey::Priority)
    {
        if (descending) mergeSort(v, [](const Artifact* x, const Artifact* y) { return *x > *y; });
        else            mergeSort(v, [](const Artifact* x, const Artifact* y) { return *x < *y; });
        return v;
    }
    auto keyOf = [key](const Artifact* a) -> double {
        return key == SortKey::Age ? a->getAge() : a->getCondition();
    };
    if (descending) mergeSort(v, [&](const Artifact* x, const Artifact* y) { return keyOf(x) > keyOf(y); });
    else            mergeSort(v, [&](const Artifact* x, const Artifact* y) { return keyOf(x) < keyOf(y); });
    return v;
}

// Exact type: "Coin" does not include ancient coins, which have their own type
ArtifactView SearchAnalytics::byType(const string& type) const
{
    return filtered([&](const Artifact& a) { return a.typeName() == type; });
}

ArtifactView SearchAnalytics::byEra(Era era) const
{
    return filtered([&](const Artifact& a) { return eraOf(a.getAge()) == era; });
}

// Inclusive at both ends
ArtifactView SearchAnalytics::byConditionRange(double lo, double hi) const
{
    return filtered([&](const Artifact& a) {
        return a.getCondition() >= lo && a.getCondition() <= hi;
    });
}

void SearchAnalytics::urgentCareReport() const
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

void SearchAnalytics::riskyStorageReport() const
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

// ---------- Movement log (read-only) ----------
// m4.c (Movement) WRITES movements.txt; Module 3 only reads it, to count moves per
// artifact and to turn real transfers into network routes. One move per line, no
// header, '|' separated, exactly the format m4.c writes:
//     MOVE|ARTIFACT_ID|FROM|TO|USER|REASON|TIMESTAMP
const char* MOVEMENT_FILE = "movements.txt";

// A missing file just means no moves are recorded yet. Malformed lines (wrong number
// of fields, missing MOVE tag, blank ID / FROM / TO) are skipped with a warning, the
// same way ArtifactStore handles a bad line in artifacts.txt.
// A line with FROM == TO is kept as a record (it is a real log entry) but it adds no
// route to the network, because connect() rejects self-loops.
vector<MovementRecord> loadMovements(const string& file)
{
    vector<MovementRecord> moves;
    ifstream in;
    if (!openDataFile(file, in)) return moves;   // throws FileAccessException if unreadable

    string line;
    int skipped = 0;
    while (getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // file saved with Windows line endings
        if (line.empty()) continue;
        vector<string> f = splitFields(line);
        if (f.size() != MOVEMENT_FIELDS || f[0] != "MOVE" || f[1].empty() || f[2].empty() || f[3].empty())
        {
            skipped++;
            continue;
        }
        moves.push_back({f[1], f[2], f[3], f[4], f[5], f[6]});
    }
    if (in.bad()) throw FileAccessException(file, "read");
    if (skipped > 0)
        cout << "Warning: skipped " << skipped << " malformed line(s) in " << file << ".\n";
    return moves;
}

map<string, int> countMovesPerArtifact(const vector<MovementRecord>& moves)
{
    map<string, int> counts;
    for (const MovementRecord& m : moves) counts[m.artifactId]++;   // a new key starts at 0
    return counts;
}

// ---------- Movement frequency report ----------
// Takes ready-made counts (artifact ID -> number of moves), so it does not depend on
// where the counts came from.
void movementFrequencyReport(const map<string, int>& movesPerArtifact)
{
    cout << "\n===== Movement Frequency Report =====\n";
    if (movesPerArtifact.empty())
    {
        cout << "No movements recorded yet (" << MOVEMENT_FILE << " is missing or empty).\n";
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

// ---------- Location network: C++ wrapper over the C Graph ----------
LocationNetwork::LocationNetwork(const string& file) : filename(file), graph(graph_new())
{
    if (graph == nullptr) throw bad_alloc();
}

LocationNetwork::~LocationNetwork() { graph_free(graph); }

void LocationNetwork::addSite(const string& site)
{
    if (graph_addSite(graph, site.c_str()) < 0) throw bad_alloc();
}

void LocationNetwork::addSitesFrom(const vector<unique_ptr<Artifact>>& items)
{
    for (const auto& a : items) addSite(a->getLocation());
}

bool LocationNetwork::hasSite(const string& site) const { return graph_find(graph, site.c_str()) >= 0; }
size_t LocationNetwork::siteCount() const { return (size_t)graph_siteCount(graph); }

// In memory only. Undirected; a self-loop or an existing connection is rejected.
bool LocationNetwork::connect(const string& a, const string& b)
{
    int rc = graph_connect(graph, a.c_str(), b.c_str());
    if (rc < 0) throw bad_alloc();
    return rc == 1;
}

// Read network.txt (one SITE_A|SITE_B per line). A missing file is normal on the
// first run; a file that exists but cannot be read throws FileAccessException.
void LocationNetwork::load()
{
    ifstream in;
    if (!openDataFile(filename, in)) return;

    string line;
    int skipped = 0;
    while (getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // file saved with Windows line endings
        if (line.empty()) continue;
        vector<string> f = splitFields(line);
        if (f.size() != 2 || f[0].empty() || f[1].empty())
        {
            skipped++;
            continue;
        }
        connect(f[0], f[1]);
    }
    if (in.bad()) throw FileAccessException(filename, "read");
    if (skipped > 0)
        cout << "Warning: skipped " << skipped << " malformed line(s) in " << filename << ".\n";
}

// Saves to disk first; only adds the connection in memory if the save worked.
// Returns false (and writes nothing) for a self-loop or an existing connection.
bool LocationNetwork::addConnection(const string& a, const string& b)
{
    int ia = graph_find(graph, a.c_str()), ib = graph_find(graph, b.c_str());
    if (a == b || (ia >= 0 && ib >= 0 && graph_hasEdge(graph, ia, ib))) return false;
    ofstream out(filename, ios::app);
    if (!out.is_open()) throw FileAccessException(filename, "write");
    out << a << "|" << b << "\n";
    out.flush();
    if (!out) throw FileAccessException(filename, "write");
    return connect(a, b);
}

// Every recorded transfer becomes a route between its two sites. Kept in memory
// only (not written to network.txt), because movements.txt already saves them.
void LocationNetwork::addEdgesFromMovements(const vector<MovementRecord>& moves)
{
    for (const MovementRecord& m : moves) connect(m.from, m.to);
}

// BFS (in C): the first time it reaches `to` is along a fewest-transfers route
vector<string> LocationNetwork::shortestPath(const string& from, const string& to) const
{
    int f = graph_find(graph, from.c_str()), t = graph_find(graph, to.c_str());
    if (f < 0 || t < 0) return {};
    vector<int> path((size_t)graph_siteCount(graph));
    int len = graph_shortestPath(graph, f, t, path.data());
    if (len < 0) throw bad_alloc();
    vector<string> names;
    for (int i = 0; i < len; i++) names.push_back(graph_siteName(graph, path[(size_t)i]));
    return names;
}

// DFS (in C): every site reachable from `site`, in visiting order
vector<string> LocationNetwork::reachableFrom(const string& site) const
{
    int s = graph_find(graph, site.c_str());
    vector<string> names;
    if (s < 0) return names;
    vector<int> order((size_t)graph_siteCount(graph));
    int n = graph_dfsOrder(graph, s, order.data());
    for (int i = 0; i < n; i++) names.push_back(graph_siteName(graph, order[(size_t)i]));
    return names;
}

int LocationNetwork::clusterCount() const
{
    int c = graph_clusterCount(graph);
    if (c < 0) throw bad_alloc();
    return c;
}

void LocationNetwork::print() const
{
    int n = graph_siteCount(graph);
    vector<int> order((size_t)n);
    for (int i = 0; i < n; i++) order[(size_t)i] = i;
    // alphabetical, so the output is the same every run
    mergeSort(order, [&](int x, int y) {
        return string(graph_siteName(graph, x)) < string(graph_siteName(graph, y));
    });

    cout << "\n=== " << n << " site(s) ===\n";
    for (int site : order)
    {
        cout << "  " << graph_siteName(graph, site) << " -> ";
        int deg = graph_degree(graph, site);
        if (deg == 0) cout << "(no connections)";
        for (int k = 0; k < deg; k++)
            cout << (k > 0 ? ", " : "") << graph_siteName(graph, graph_neighbor(graph, site, k));
        cout << "\n";
    }
}

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
        cout << "1. Artifact (generic)\n2. Pottery\n3. Coin\n4. Ancient Coin\n"
             << "5. Weapon\n6. Sculpture\n7. Manuscript\n";
        static const char* types[] = {"Artifact", "Pottery", "Coin", "Ancient Coin",
                                      "Weapon", "Sculpture", "Manuscript"};
        string type = types[readInt("Select type: ", 1, 7) - 1];
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
    else movementFrequencyReport(countMovesPerArtifact(loadMovements()));
}

void printRoute(const vector<string>& path)
{
    for (size_t i = 0; i < path.size(); i++)
        cout << (i > 0 ? " -> " : "  ") << path[i];
    cout << "\n";
}

// Sites come from the artifacts' locations, the connections saved in network.txt,
// and the transfers recorded in movements.txt
void networkMenu(const vector<unique_ptr<Artifact>>& items)
{
    LocationNetwork net;
    net.addSitesFrom(items);
    net.load();                                  // throws FileAccessException if network.txt is unreadable
    net.addEdgesFromMovements(loadMovements());  // same for movements.txt

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
// Takes the store's items rather than ArtifactStore itself, so Module 3 never needs
// ArtifactStore (m1.h) at all.
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
