// m3_harness.cpp - automated checks for Module 3, the pure-C core, passwords and the class
// hierarchy (NOT part of the app build).
// Build (from the repo root):   make m3_harness
// Run it from an empty scratch folder: it writes its own data files there.
#include "../m0.h"
#include "../m1.h"
#include "../m2.h"
#include "../m3.h"
#include "../m3_core.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
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

    net.addEdgesFromMovements({{"X1", "D", "E", "Curator", "Loan", "2026-10-01 09:00:00"},
                               {"X2", "F", "G", "Curator", "Loan", "2026-10-02 09:00:00"}});
    check(net.clusterCount() == 1, "movement routes join everything into 1 cluster");
    check(net.shortestPath("A", "G").size() == 5, "after hook, A->G is A-D-E-F-G (4 transfers)");
}

void checkMovementLog()
{
    cout << "\n[7] Movement log (movements.txt) and frequency report\n";

    vector<MovementRecord> none = loadMovements("no_movements_here.txt");
    check(none.empty(), "missing movements file -> no moves, no exception");

    // Binary mode so the "\r\n" line really reaches the file on Windows too
    ofstream out("harness_movements.txt", ios::binary);
    // Exactly the format m4.c writes: MOVE|id|from|to|user|reason|timestamp
    out << "MOVE|PT01|Delhi Museum|Dehradun Lab|Dr Rao|Restoration|2026-10-01 09:00:00\n"
        << "MOVE|A001|Patna Vault|Delhi Museum|Curator|Exhibition|2026-10-04 10:00:00\r\n"   // Windows line ending
        << "MOVE|A001|Delhi Museum|Patna Vault|Curator|Exhibition ended|2026-10-11 11:00:00\n"
        << "\n"                                                                      // blank line: ignored
        << "MOVE|C002|Store Room B|Patna Vault\n"                                    // too few fields
        << "COPY|C003|Delhi Museum|Patna Vault|Curator|Wrong tag|2026-10-15 12:00:00\n" // not a MOVE line
        << "MOVE||Delhi Museum|Patna Vault|Curator|No ID|2026-10-15 12:00:00\n"       // blank ID
        << "MOVE|PT01|Dehradun Lab|Delhi Museum|Dr Rao|Returned|2026-10-09 13:00:00\n";
    out.close();

    vector<MovementRecord> moves = loadMovements("harness_movements.txt");
    check(moves.size() == 4, "4 valid moves read, 3 malformed lines skipped, got " + to_string(moves.size()));
    check(moves.size() > 1 && moves[1].reason == "Exhibition", "trailing \\r removed from a Windows line");

    map<string, int> counts = countMovesPerArtifact(moves);
    check(counts.size() == 2 && counts["A001"] == 2 && counts["PT01"] == 2, "counts: A001 2, PT01 2");

    LocationNetwork net("unused_network.txt");
    net.addEdgesFromMovements(moves);
    check(net.shortestPath("Dehradun Lab", "Patna Vault").size() == 3,
          "movement routes give Dehradun Lab -> Delhi Museum -> Patna Vault");

    filesystem::create_directory("movements_is_a_folder");
    try { loadMovements("movements_is_a_folder"); check(false, "unreadable movements file throws"); }
    catch (const FileAccessException& e) { check(true, string("unreadable movements file -> FileAccessException: ") + e.what()); }

    movementFrequencyReport(counts);
    cout << "  (expect A001 2 then PT01 2: a tie stays in ID order)\n";
    movementFrequencyReport({{"A001", 3}, {"C002", 7}, {"G001", 3}, {"PT01", 1}, {"C001", 7}});
    cout << "  (expect C001 7, C002 7, A001 3, G001 3, PT01 1: ties stay in ID order)\n";
    movementFrequencyReport({});
}

// ---------- New checks: pure-C core used directly, hierarchy, passwords ----------
struct Pair { int key; int original; };
int comparePairs(const void* a, const void* b, void*)
{
    int x = static_cast<const Pair*>(a)->key, y = static_cast<const Pair*>(b)->key;
    return x < y ? -1 : (x > y ? 1 : 0);
}

void checkPureCCore()
{
    cout << "\n[8] Pure-C core (m3_core.c) called directly\n";

    // hash table
    HashIndex h;
    check(hi_init(&h) == 0, "hi_init succeeds");
    int values[200];
    bool allInserted = true;
    for (int i = 0; i < 200; i++)
    {
        values[i] = i;
        allInserted = allInserted && hi_insert(&h, ("K" + to_string(i)).c_str(), &values[i]) == HI_OK;
    }
    check(allInserted && h.count == 200, "200 keys inserted");
    check(hi_loadFactor(&h) <= 0.75 && h.rehashes > 0, "load factor <= 0.75 after rehashing");
    check(hi_insert(&h, "K7", &values[7]) == HI_DUPLICATE, "duplicate key rejected with HI_DUPLICATE");
    check(hi_find(&h, "K42") == &values[42] && hi_find(&h, "nope") == nullptr, "find hits and misses");
    hi_free(&h);

    // merge sort: sorted + stable on a few thousand pairs with many equal keys
    vector<Pair> v;
    for (int i = 0; i < 3000; i++) v.push_back({(i * 7919) % 50, i});
    check(merge_sort(v.data(), v.size(), sizeof(Pair), comparePairs, nullptr) == 0, "merge_sort returns 0");
    bool sorted = true, stable = true;
    for (size_t i = 1; i < v.size(); i++)
    {
        if (v[i - 1].key > v[i].key) sorted = false;
        if (v[i - 1].key == v[i].key && v[i - 1].original > v[i].original) stable = false;
    }
    check(sorted, "merge_sort: output is in order");
    check(stable, "merge_sort: equal keys keep their original order (stable)");
    check(merge_sort(v.data(), 0, sizeof(Pair), comparePairs, nullptr) == 0 &&
          merge_sort(v.data(), 1, sizeof(Pair), comparePairs, nullptr) == 0, "empty and 1-element arrays are fine");

    // graph
    Graph* g = graph_new();
    graph_connect(g, "A", "B"); graph_connect(g, "B", "C"); graph_connect(g, "C", "D"); graph_connect(g, "A", "D");
    graph_connect(g, "E", "F"); graph_addSite(g, "G");
    check(graph_connect(g, "B", "A") == 0 && graph_connect(g, "A", "A") == 0, "graph rejects duplicate edge and self-loop");
    int path[16];
    int len = graph_shortestPath(g, graph_find(g, "B"), graph_find(g, "D"), path);
    check(len == 3, "BFS B->D is 2 transfers");
    check(graph_shortestPath(g, graph_find(g, "A"), graph_find(g, "E"), path) == 0, "BFS across clusters returns 0");
    int order[16];
    check(graph_dfsOrder(g, graph_find(g, "C"), order) == 4, "DFS from C reaches 4 sites");
    check(graph_clusterCount(g) == 3, "3 clusters");
    graph_free(g);
}

void checkHierarchyAndOperators()
{
    cout << "\n[9] Class hierarchy: multiple/multilevel/hybrid inheritance and operators\n";
    PotteryArtifact pot("P1", "Jar", 4500, 3.5, "Delhi", 0.6);
    CoinArtifact bronze("C1", "Bronze coin", 900, 5.0, "Delhi", "Bronze");
    AncientCoin maurya("A1", "Punch-marked", 2300, 4.0, "Patna", "Silver", "Maurya");
    Artifact plain("G1", "Tablet", 3000, 5.0, "Patna");

    const Artifact* base = &maurya;
    const Artifact* asPot = &pot;
    const Artifact* asBronze = &bronze;
    const Artifact* asPlain = &plain;
    check(dynamic_cast<const CoinArtifact*>(base) != nullptr, "multilevel: AncientCoin is a CoinArtifact is an Artifact");
    check(dynamic_cast<const Restorable*>(asPot) != nullptr && dynamic_cast<const Restorable*>(asBronze) != nullptr,
          "multiple inheritance: pottery and coins are also Restorable");
    check(dynamic_cast<const Restorable*>(asPlain) == nullptr, "a plain Artifact is not Restorable");
    check(dynamic_cast<const Restorable*>(base) != nullptr, "hybrid: AncientCoin reaches Restorable through CoinArtifact");
    check(base->typeName() == "Ancient Coin" && base->calculatePriority() > maurya.CoinArtifact::calculatePriority(),
          "virtual dispatch: AncientCoin priority adds the dynasty bonus");
    check(bronze.restorationAdvice().find("bronze disease") != string::npos, "bronze coin advice mentions bronze disease");
    check(maurya.restorationAdvice().find("Document it fully") != string::npos, "AncientCoin extends the coin advice");

    check(plain < pot && pot > plain && !(pot < plain), "operator< / operator> compare by priority");
    PotteryArtifact samePot("P1", "Other name", 10, 9.0, "X", 0.1);
    check(pot == samePot && !(pot == plain), "operator== compares IDs");
    ostringstream os;
    os << maurya;
    check(os.str() == "A1 (Ancient Coin, Punch-marked) at Patna", "operator<< prints a one-line summary: " + os.str());

    plain.setLocation("Delhi").setLocation("Kolkata");
    check(plain.getLocation() == "Kolkata", "setLocation returns *this, so calls chain");

    // serialize -> parse round trip for all four types
    const vector<const Artifact*> everyType = {asPot, asBronze, base, asPlain};
    for (const Artifact* a : everyType)
    {
        unique_ptr<Artifact> back = parseArtifact(a->serialize());
        check(back && back->serialize() == a->serialize(), "round trip: " + a->typeName());
    }
}

void checkPasswords()
{
    cout << "\n[10] Passwords\n";
    check(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256(\"abc\") matches the standard test vector");
    check(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256(\"\") matches the standard test vector");
    string r1 = makePasswordRecord("asd123"), r2 = makePasswordRecord("asd123");
    check(r1 != r2, "same password, different salt: records differ");
    check(verifyPassword("asd123", r1) && verifyPassword("asd123", r2), "correct password verifies");
    check(!verifyPassword("asd124", r1) && !verifyPassword("", r1), "wrong password is rejected");
    check(r1.find("asd123") == string::npos, "the plain password does not appear in the record");
    check(!verifyPassword("asd123", "asd123"), "a plain-text record never verifies");
}

void checkNewTypes()
{
    cout << "\n[11] New types: Weapon, Sculpture, Manuscript\n";
    Weapon w("W1", "Iron Sword", 1000, 4.0, "Delhi", 0.5);
    Sculpture s("S1", "Stone Lion", 1500, 5.0, "Patna", 0.2);
    Manuscript m("M1", "Palm Leaf", 600, 6.0, "Kolkata", 0.9);
    const Artifact* aw = &w;
    const Artifact* as = &s;
    const Artifact* am = &m;

    // priority = base + (10 - condition) * index * 5
    check(fixed2(aw->calculatePriority()) == "54.00", "Weapon priority 39 + 15 = 54.00, got " + fixed2(aw->calculatePriority()));
    check(fixed2(as->calculatePriority()) == "47.50", "Sculpture priority 42.5 + 5 = 47.50, got " + fixed2(as->calculatePriority()));
    check(fixed2(am->calculatePriority()) == "43.00", "Manuscript priority 25 + 18 = 43.00, got " + fixed2(am->calculatePriority()));
    check(aw->typeName() == "Weapon" && as->typeName() == "Sculpture" && am->typeName() == "Manuscript", "typeName labels");

    check(dynamic_cast<const Restorable*>(aw) && dynamic_cast<const Restorable*>(as) && dynamic_cast<const Restorable*>(am),
          "all three are Restorable (multiple inheritance with Artifact)");
    check(w.restorationAdvice().find("Heavy rust") != string::npos, "rust 0.5 or more gives the heavy-rust advice");
    check(Weapon("W2", "x", 10, 5.0, "L", 0.1).restorationAdvice().find("Light rust") != string::npos, "low rust gives the light-rust advice");
    check(s.restorationAdvice().find("Stable") != string::npos && m.restorationAdvice().find("Advanced decay") != string::npos,
          "sculpture and manuscript advice follow their index");

    check(w.serialize() == "WEAPON|W1|Iron Sword|1000|4.000000|Delhi|0.500000", "Weapon serializes as WEAPON|... : " + w.serialize());
    for (const Artifact* a : {aw, as, am})
    {
        unique_ptr<Artifact> back = parseArtifact(a->serialize());
        check(back && back->serialize() == a->serialize() && back->typeName() == a->typeName(), "round trip: " + a->typeName());
    }
    check(!parseArtifact("WEAPON|W1|Iron Sword|1000|4.0|Delhi"), "WEAPON line with a missing field is rejected");
    check(!parseArtifact("SCULPTURE|S1|Lion|1500|5.0|Patna|abc"), "SCULPTURE line with a non-numeric index is rejected");
    check(!parseArtifact("MANUSCRIPT|M1|Leaf|600|6.0|Kolkata|0.9|extra"), "MANUSCRIPT line with an extra field is rejected");

    check(*as < *aw && *aw > *am && *am < *as, "operators compare priority across the new types");

    // Module 3 sees them like any other type
    vector<unique_ptr<Artifact>> items;
    items.push_back(make_unique<Weapon>("W1", "Iron Sword", 1000, 4.0, "Delhi", 0.5));
    items.push_back(make_unique<Sculpture>("S1", "Stone Lion", 1500, 5.0, "Patna", 0.2));
    items.push_back(make_unique<Manuscript>("M1", "Palm Leaf", 600, 6.0, "Kolkata", 0.9));
    items.push_back(make_unique<PotteryArtifact>("P1", "Jar", 1200, 4.0, "Delhi", 0.6));
    SearchAnalytics sa(items);
    check(sa.byType("Weapon").size() == 1 && sa.byType("Sculpture").size() == 1 && sa.byType("Manuscript").size() == 1,
          "filter by type finds each new type");
    check(sa.byType("Pottery").size() == 1, "the new types do not leak into the Pottery filter");
    ArtifactView byP = sa.sortedBy(SortKey::Priority, true);
    check(byP.size() == 4 && byP[0]->getId() == "P1" && byP[1]->getId() == "W1", "sorted by priority across all four types: " + byP[0]->getId() + ", " + byP[1]->getId());
    check(sa.findById("M1").getName() == "Palm Leaf", "hash lookup finds a Manuscript");
}

int main()
{
    const int N = 5000;
    cout << "===== Module 3 / core harness =====\n";
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
    checkMovementLog();
    checkPureCCore();
    checkHierarchyAndOperators();
    checkPasswords();
    checkNewTypes();

    cout << "\n===== " << passed << " passed, " << failed << " failed =====\n";
    return failed == 0 ? 0 : 1;
}
