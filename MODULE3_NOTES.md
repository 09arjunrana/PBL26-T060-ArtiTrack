# Module 3 — Search, Sorting & Graph Analytics (viva notes)

Code: `m4.cpp` (menu: Artifact Menu → 4. Search, Sort & Reports). Tests: `tests/`.
All of Module 3 is **read-only**: it holds non-owning `const Artifact*` pointers into
`ArtifactStore`'s `vector<unique_ptr<Artifact>>` and never reorders, copies or deletes them.
Those pointers stay valid when the vector grows, because `unique_ptr` moves; the artifacts don't.

| Structure / algorithm | Problem it solves | Time | Alternative rejected, and why |
|---|---|---|---|
| **Hash table, separate chaining** (`ArtifactHashIndex`) | "Find artifact #4471" without scanning everything | avg O(1) find/insert; worst O(n) if all IDs collide; rebuild O(n) | `std::unordered_map`: same idea, but hides the hashing we must demonstrate. Open addressing: clusters and must stay well below full. Linear scan (`ArtifactStore::find`): O(n). |
| **djb2 hash** `h = h*33 + c` | Turn an ID string into a bucket number | O(length of ID) | Summing characters: anagrams like `AB1`/`BA1` collide. |
| **Rehash at load factor > 0.75**, to 2n+1 buckets | Keep chains short as data grows | O(n) per rehash, amortised O(1) per insert (doubling) | Fixed bucket count: chains grow, so lookup slides back towards O(n). |
| **Merge sort**, generic with a comparator (`mergeSort`) | Order reports by age / condition / priority, ascending or descending | O(n log n) always; O(n) extra memory | Quicksort: in place, but not stable and O(n²) worst case. `std::sort`: not stable, and hides the algorithm. |
| **Stability** (take left on ties) | Equal keys keep save-file order, so reports are predictable | — | An unstable sort could shuffle tied artifacts between runs. |
| **Filters** (type, era, condition range) | "Everything ancient", "condition 0–5" | O(n) single pass | BST on age: O(log n + k), but not in scope for this version (see below). |
| **Risk banding** (urgent care report) | Turn priority scores into Low / Medium / High | O(n log n) (sort, then one pass) | Thirds of the full score range: age dominates (0–1590), so almost all would be Low. |
| **Group by location** (`map<string, LocationStats>`) | Count, average condition and priority per site; flag risky storage | O(n log L) for L locations | Nested loops over locations × artifacts: O(n·L). |
| **Graph as adjacency list** (`map<string, vector<string>>`) | How storage sites connect | memory O(V + E) | Adjacency matrix: O(V²) memory, mostly empty for few routes. |
| **BFS** (`shortestPath`) | Fewest transfers between two sites | O(V + E) | DFS finds *a* path, not the shortest. Dijkstra is unnecessary because every hop costs the same. |
| **DFS** (`reachableFrom`, `clusterCount`) | All sites reachable from one site; number of separate clusters | O(V + E) | Union-find counts clusters but cannot list reachable sites in order. |

**Risk thresholds (named constants in `m4.cpp`).** Base priority = (10 − condition)·4 + age/100·1.5, so condition
alone gives 0–40. **Medium ≥ 20** is what a generic, age-0 artifact at condition 5 scores; **High ≥ 40** is the
same at condition 0. Age then adds 1.5 per century. The low/medium/high *idea* comes from the English Heritage
risk & condition audit in our literature survey; **the numbers are ours**, not English Heritage's. A storage location
is flagged when its *average* priority is ≥ 40. Era bands: Modern < 200, Early Modern 200–499, Medieval 500–1499,
Ancient ≥ 1500 years (our own rough cut-offs).

**OOP points.** Three custom exceptions (`ArtifactNotFoundException`, `DuplicateIdException`, `FileAccessException`)
derive from `std::runtime_error`; they are thrown inside Module 3 and caught in one place (`runGuarded`), so
the program never crashes on them. The friend function `printReportRow` reads Artifact's fields directly for
the report table. Honest caveat: the new getters would also have worked, so the friend mainly demonstrates
the syllabus concept (CO2). `typeName()` is a virtual function overridden per subclass. `SearchAnalytics` has one job:
answer read-only questions about the collection.

**Measured** (`tests/m4_harness.cpp`, 5,000 artifacts, g++ 15.2 -O2, this laptop; your numbers will vary):
hash lookup **12.4 ns** vs linear **5,664 ns** (about 457× faster). The index went 17 → 9,215 buckets over
9 rehashes, final load factor 0.54, longest chain 2. Rebuild 0.58 ms. Merge sort of 5,000 by priority:
0.92 ms (`std::stable_sort` 0.85 ms). For every key and both directions, the output is sorted and identical to
`std::stable_sort`, which proves stability. 32/32 checks pass.

**Waiting on teammates.** Movement frequency: `movementFrequencyReport(map<ID, moves>)` is written and tested,
but the menu says "Movement data not available yet" until Module 2's log exists (`TODO(integration): Module 2`).
Graph hook: `LocationNetwork::addEdgesFromMovements(from/to pairs)`.

**Known gap.** The Phase-I slide lists **BST/AVL (U3)** under M3; this version does not implement one.
A BST keyed on age would serve the era and range filters in O(log n + k).
