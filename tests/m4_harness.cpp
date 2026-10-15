// m4_harness.cpp - automated checks for Module 3 (NOT part of the app build)
// Build (from the repo root): g++ -std=c++17 -Wall -Wextra -O2 tests/m4_harness.cpp -o m4_harness
// Run it from an empty scratch folder: it writes its own data files there.
#define main artitrack_main   // reuse m1.cpp's ArtifactStore without its main()
#include "../m1.cpp"
#undef main
#include <chrono>
#include <cstdint>
using namespace std;

// ---------- Tiny check framework ----------
int passed = 0, failed = 0;

void check(bool ok, const string& what)
{
    cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (ok) passed++;
    else failed++;
}

double msSince(chrono::steady_clock::time_point start)
{
    return chrono::duration<double, milli>(chrono::steady_clock::now() - start).count();
}

// ---------- Test data ----------
// Deterministic data with many repeated ages / conditions, so stability is really tested
void writeLargeDataFile(const string& file, int n)
{
    static const char* sites[]  = {"Delhi Museum", "Store Room B", "Dehradun Lab", "Patna Vault",
                                   "Kolkata Annex", "Field Camp", "Chennai Depot", "Mumbai Gallery"};
    static const char* metals[] = {"Gold", "Silver", "Bronze", "Copper"};
    ofstream out(file);
    for (int i = 1; i <= n; i++)
    {
        ostringstream id;
        id << "H" << setw(5) << setfill('0') << i;
        int age = (i * 7919) % 4000;
        double cond = (i * 13 % 21) / 2.0;   // 0.0 .. 10.0 in steps of 0.5
        string loc = sites[i % 8];
        unique_ptr<Artifact> a;
        if (i % 4 == 0)      a = make_unique<Artifact>(id.str(), "Generic", age, cond, loc);
        else if (i % 4 == 1) a = make_unique<PotteryArtifact>(id.str(), "Pot", age, cond, loc, (i % 11) / 10.0);
        else if (i % 4 == 2) a = make_unique<CoinArtifact>(id.str(), "Coin", age, cond, loc, metals[i % 4]);
        else                 a = make_unique<AncientCoin>(id.str(), "Old coin", age, cond, loc,
                                                          metals[i % 4], i % 3 == 0 ? "Maurya" : "Chola");
        out << a->serialize() << "\n";
    }
}

// ---------- Checks ----------
void checkHashMatchesLinear(const ArtifactStore& store, const SearchAnalytics& sa)
{
    cout << "\n[1] Hash lookup returns the same object as ArtifactStore::find\n";
    int mismatches = 0;
    for (const auto& a : store.all())
        if (&sa.findById(a->getId()) != store.find(a->getId())) mismatches++;
    check(mismatches == 0, "all " + to_string(store.size()) + " IDs: same pointer from hash and linear find");

    int missesThrown = 0, linearNull = 0;
    for (int i = 0; i < 1000; i++)
    {
        string id = "MISSING" + to_string(i);
        if (store.find(id) == nullptr) linearNull++;
        try { sa.findById(id); }
        catch (const ArtifactNotFoundException&) { missesThrown++; }
    }
    check(missesThrown == 1000 && linearNull == 1000,
          "1000 unknown IDs: hash throws ArtifactNotFoundException, linear returns nullptr");
}

void checkRehashing(const ArtifactStore& store)
{
    cout << "\n[2] Rehashing while 5000 IDs are inserted (load factor limit 0.75)\n";
    ArtifactHashIndex idx;
    size_t lastBuckets = idx.bucketCount();
    cout << "  start: " << lastBuckets << " buckets\n";
    for (const auto& a : store.all())
    {
        idx.insert(a.get());
        if (idx.bucketCount() != lastBuckets)
        {
            cout << "  after insert #" << setw(4) << idx.size() << ": rehashed "
                 << setw(4) << lastBuckets << " -> " << setw(5) << idx.bucketCount() << " buckets\n";
            lastBuckets = idx.bucketCount();
        }
    }
    cout << "  final: " << idx.size() << " IDs, " << idx.bucketCount() << " buckets, load factor "
         << fixed2(idx.loadFactor()) << ", longest chain " << idx.longestChain() << ", "
         << idx.rehashes() << " rehashes\n";
    check(idx.loadFactor() <= 0.75, "load factor stays <= 0.75 after every insert");
    check(idx.rehashes() > 0, "rehash actually happened");
}

void checkTimings(const ArtifactStore& store, const SearchAnalytics& sa)
{
    cout << "\n[3] Timing: hash vs linear lookup (" << store.size() << " artifacts, -O2)\n";
    vector<string> ids;
    for (const auto& a : store.all()) ids.push_back(a->getId());
    uintptr_t sink = 0;   // use every result so the compiler cannot skip the lookups

    const int hashRounds = 200, linearRounds = 4;
    auto t0 = chrono::steady_clock::now();
    for (int r = 0; r < hashRounds; r++)
        for (const string& id : ids) sink ^= reinterpret_cast<uintptr_t>(&sa.findById(id));
    double hashMs = msSince(t0);

    t0 = chrono::steady_clock::now();
    for (int r = 0; r < linearRounds; r++)
        for (const string& id : ids) sink ^= reinterpret_cast<uintptr_t>(store.find(id));
    double linearMs = msSince(t0);

    double hashNs = hashMs * 1e6 / (double(hashRounds) * ids.size());
    double linearNs = linearMs * 1e6 / (double(linearRounds) * ids.size());
    cout << "  hash   : " << hashRounds * ids.size() << " lookups in " << fixed2(hashMs) << " ms  = "
         << fixed2(hashNs) << " ns per lookup\n"
         << "  linear : " << linearRounds * ids.size() << " lookups in " << fixed2(linearMs) << " ms  = "
         << fixed2(linearNs) << " ns per lookup\n"
         << "  hash is about " << fixed2(linearNs / hashNs) << "x faster per lookup"
         << "  (sink " << (sink & 1) << ")\n";

    // Rebuild cost: paid once each time Module 3 opens
    t0 = chrono::steady_clock::now();
    ArtifactHashIndex idx;
    idx.rebuild(store.all());
    cout << "  index rebuild for " << idx.size() << " artifacts: " << fixed2(msSince(t0)) << " ms\n";

    // Sort cost: our merge sort vs std::stable_sort on the same view
    ArtifactView v1 = sa.all(), v2 = sa.all();
    auto byPriority = [](const Artifact* x, const Artifact* y) {
        return x->calculatePriority() < y->calculatePriority();
    };
    t0 = chrono::steady_clock::now();
    mergeSort(v1, byPriority);
    double ours = msSince(t0);
    t0 = chrono::steady_clock::now();
    stable_sort(v2.begin(), v2.end(), byPriority);
    cout << "  sort by priority: our merge sort " << fixed2(ours) << " ms, std::stable_sort "
         << fixed2(msSince(t0)) << " ms\n";
}

void checkSorting(const SearchAnalytics& sa)
{
    cout << "\n[4] Sorting: every key, both directions, sorted AND stable\n";
    struct Case { SortKey key; const char* name; };
    const Case cases[] = {{SortKey::Age, "age"}, {SortKey::Condition, "condition"},
                          {SortKey::Priority, "priority"}};
    for (const Case& c : cases)
    {
        auto keyOf = [&](const Artifact* a) -> double {
            if (c.key == SortKey::Age)       return a->getAge();
            if (c.key == SortKey::Condition) return a->getCondition();
            return a->calculatePriority();
        };
        for (bool desc : {false, true})
        {
            ArtifactView ours = sa.sortedBy(c.key, desc);
            bool inOrder = true;
            for (size_t i = 1; i < ours.size(); i++)
            {
                double prev = keyOf(ours[i - 1]), cur = keyOf(ours[i]);
                if (desc ? prev < cur : prev > cur) inOrder = false;
            }
            // std::stable_sort is guaranteed stable; same input + same comparator must
            // give exactly the same order if our merge sort is stable too
            ArtifactView reference = sa.all();
            stable_sort(reference.begin(), reference.end(), [&](const Artifact* x, const Artifact* y) {
                return desc ? keyOf(x) > keyOf(y) : keyOf(x) < keyOf(y);
            });
            string label = string(c.name) + (desc ? " descending" : " ascending");
            check(inOrder, label + ": sorted");
            check(ours == reference, label + ": identical to std::stable_sort (so ties keep store order)");
        }
    }
}

void checkStoreUntouched(const ArtifactStore& store, const vector<const Artifact*>& before)
{
    vector<const Artifact*> after;
    for (const auto& a : store.all()) after.push_back(a.get());
    check(after == before, "store's own item order is unchanged after all six sorts");
}

void checkExceptions(const ArtifactStore& store)
{
    cout << "\n[5] Exception paths\n";
    ArtifactHashIndex idx;
    idx.rebuild(store.all());

    try { idx.find("NOPE"); check(false, "unknown ID throws"); }
    catch (const ArtifactNotFoundException& e) { check(true, string("unknown ID -> ArtifactNotFoundException: ") + e.what()); }

    try { idx.insert(store.all().front().get()); check(false, "duplicate ID throws"); }
    catch (const DuplicateIdException& e) { check(true, string("duplicate ID -> DuplicateIdException: ") + e.what()); }

    // A folder where network.txt should be: exists but cannot be read as a file
    filesystem::create_directory("net_is_a_folder");
    try { LocationNetwork net("net_is_a_folder"); net.load(); check(false, "unreadable file throws"); }
    catch (const FileAccessException& e) { check(true, string("unreadable network file -> FileAccessException: ") + e.what()); }

    // Saving into a folder that does not exist
    try
    {
        LocationNetwork net("no_such_folder/network.txt");
        net.addConnection("A", "B");
        check(false, "unwritable file throws");
    }
    catch (const FileAccessException& e) { check(true, string("missing folder on save -> FileAccessException: ") + e.what()); }

    LocationNetwork fresh("missing_network.txt");
    bool threw = false;
    try { fresh.load(); }
    catch (const FileAccessException&) { threw = true; }
    check(!threw && fresh.siteCount() == 0, "missing network.txt on first run is not an error (empty network)");

    // All three derive from runtime_error, as required
    try { throw DuplicateIdException("X"); }
    catch (const runtime_error&) { check(true, "Module 3 exceptions are caught as std::runtime_error"); }
}

void checkGraph()
{
    cout << "\n[6] Location network: BFS and DFS on a known graph\n";
    LocationNetwork net("graph_test.txt");
    //  A - B - C - D      E - F      G (isolated)
    //   \_________/
    net.connect("A", "B"); net.connect("B", "C"); net.connect("C", "D"); net.connect("A", "D");
    net.connect("E", "F"); net.addSite("G");
    check(!net.connect("B", "A"), "duplicate edge (reversed) rejected");
    check(!net.connect("A", "A"), "self-loop rejected");

    vector<string> p = net.shortestPath("B", "D");
    check(p.size() == 3, "BFS B->D takes 2 transfers (B-A-D or B-C-D), got " + to_string(p.size() - 1));
    check(net.shortestPath("A", "E").empty(), "BFS between clusters: no route");
    check(net.shortestPath("A", "A").size() == 1, "BFS from a site to itself: 0 transfers");
    check(net.reachableFrom("C").size() == 4, "DFS from C reaches A, B, C, D");
    check(net.clusterCount() == 3, "3 clusters: {A,B,C,D}, {E,F}, {G}");

    net.addEdgesFromMovements({{"D", "E"}, {"F", "G"}});
    check(net.clusterCount() == 1, "movement hook joins everything into 1 cluster");
    check(net.shortestPath("A", "G").size() == 5, "after hook, A->G is A-D-E-F-G (4 transfers)");
}

void checkMovementReport()
{
    cout << "\n[7] Movement frequency report (sample counts until Module 2 exists)\n";
    movementFrequencyReport({{"A001", 3}, {"C002", 7}, {"G001", 3}, {"PT01", 1}, {"C001", 7}});
    cout << "  (expect C001 7, C002 7, A001 3, G001 3, PT01 1: ties stay in ID order)\n";
    movementFrequencyReport({});
}

int main()
{
    const int N = 5000;
    cout << "===== Module 3 harness =====\n";
    writeLargeDataFile("harness_artifacts.txt", N);
    ArtifactStore store("harness_artifacts.txt");
    store.load();
    cout << "Loaded " << store.size() << " generated artifacts.\n";

    vector<const Artifact*> before;
    for (const auto& a : store.all()) before.push_back(a.get());

    SearchAnalytics sa(store.all());
    checkHashMatchesLinear(store, sa);
    checkRehashing(store);
    checkTimings(store, sa);
    checkSorting(sa);
    checkStoreUntouched(store, before);
    checkExceptions(store);
    checkGraph();
    checkMovementReport();

    cout << "\n===== " << passed << " passed, " << failed << " failed =====\n";
    return failed == 0 ? 0 : 1;
}
