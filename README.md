# ArtiTrack — Artifact Movement & Preservation Tracker (Team 060)

Console application in C and C++17 (Data Structures with C + OOP with C++).
Registers artifacts, logs every transfer between sites, ranks restoration need, and
answers search / sort / route questions. Everything is saved in flat text files.

## Build and run

```
make            # builds ./artitrack   (Windows: mingw32-make, or ./build.sh in Git Bash)
./artitrack
make test       # builds the test harness, then runs every test (needs bash)
make clean
```
Needs `gcc` and `g++` (C11 / C++17). No other libraries.

## Files and modules

| File | Language | Role |
|---|---|---|
| `main.cpp` | C++ | The hub: the only `main()`. Login menu and artifact menu, routes to every module. |
| `m0.h / m0.cpp` | C++ | **Login** (register/login, salted SHA-256 passwords) and the shared input helpers. |
| `m1.h / m1.cpp` | C++ | **Module 1 – Core & persistence**: `ArtifactStore` owns every artifact and keeps `artifacts.txt` in sync. |
| `m2.h / m2.cpp` | C++ | **Class hierarchy**: `Artifact`, `PotteryArtifact`, `CoinArtifact`, `AncientCoin`, `Weapon`, `Sculpture`, `Manuscript`, mixin `Restorable`, operators. |
| `m3_core.h / m3_core.c` | **pure C** | **Module 3 data structures**: hash table, stable merge sort, graph with BFS/DFS. |
| `m3.h / m3.cpp` | C++ | **Module 3 – Search, sort & analytics**: wraps the C code, exceptions, filters, reports, menus. |
| `m4.h / m4.c` | **pure C** | **Module 2 (Phase-I name) – Movement & restoration**: linked list, undo stack, priority heap. |
| `bridge.h / bridge.cpp` | C++ | The C-callable functions `m4.c` uses to reach `ArtifactStore` (plain C types only). |
| `tests/` | | `m3_harness.cpp` (94 unit checks), `run_tests.sh` (55 end-to-end menu checks), sample data. |
| `MODULE3_NOTES.md` | | Viva notes: why each structure was chosen and what was rejected. |
| `CHANGES.md` | | What changed from the previous version and why. |

The code comments call the movement module "Module 4" and the search module "Module 3";
the Phase-I slides called them Module 2 and Module 3. Same code, two numbering schemes.

### Where each syllabus concept lives

| Concept | Where |
|---|---|
| Linked list | `m4.c` movement log (`MovementNode`, head + tail) |
| Stack | `m4.c` undo stack (`UndoNode`) |
| Binary heap / priority queue | `m4.c` restoration queue (`RestoreHeap`) |
| Queue | `m3_core.c` BFS queue |
| Hashing | `m3_core.c` `HashIndex` (djb2, chaining, rehash) |
| Sorting | `m3_core.c` `merge_sort` (stable) |
| Graph, BFS, DFS | `m3_core.c` `Graph` |
| Arrays / vectors, file I/O | `m1.cpp`, `m3.cpp`, `m4.c` |
| Constructors, `this` pointer | `m2.cpp` (`setLocation` returns `*this`) |
| Operator overloading | `m2.h/.cpp`: `<`, `>`, `==`, `<<` on `Artifact` |
| Single / multilevel / multiple / hybrid inheritance | `m2.h` (see the diagram at the top) |
| Virtual functions, polymorphism | `calculatePriority`, `display`, `serialize`, `restorationAdvice` |
| Friend function | `printReportRow` (declared in `m2.h`, defined in `m3.cpp`) |
| Exceptions | `m3.h` (three classes from `runtime_error`), caught in `runGuarded` |
| Templates, STL | `mergeSort` / `filtered` templates; `vector`, `map`, `unique_ptr`, `unordered_set` |

## Data files (all `|` separated, one record per line)

```
artifacts.txt  POTTERY|id|name|age|condition|location|porosity
               COIN|id|name|age|condition|location|metal
               ANCIENT|id|name|age|condition|location|metal|dynasty
               WEAPON|id|name|age|condition|location|rustIndex
               SCULPTURE|id|name|age|condition|location|crackIndex
               MANUSCRIPT|id|name|age|condition|location|decayIndex
               ARTIFACT|id|name|age|condition|location
movements.txt  MOVE|id|from|to|user|reason|timestamp          (written by m4.c, read by m3.cpp)
network.txt    siteA|siteB                                      (optional extra routes)
users.txt      username sha256$<salt>$<hash>
```

## Artifact types and priority

| Type | Extra field (0.0 - 1.0) | Priority = base + ... |
|---|---|---|
| `Artifact` (generic) | none | `(10 - condition) * 4 + (age / 100) * 1.5` |
| `PotteryArtifact` | porosity | `(10 - condition) * porosity * 5` |
| `CoinArtifact` | metal | `+ 12` for Bronze or Copper |
| `AncientCoin` | metal + dynasty | coin score `+ 20` (Maurya, Gupta) or `+ 10` |
| `Weapon` | rust index | `(10 - condition) * rust * 5` |
| `Sculpture` | crack index | `(10 - condition) * crack * 5` |
| `Manuscript` | decay index | `(10 - condition) * decay * 5` |

Pottery, coins, weapons, sculptures and manuscripts also inherit the `Restorable` mixin, so each gives
conservation advice (shown in search-by-ID and in the restoration queue).

## Menus

```
Artifact menu: 1 Add  2 Display all  3 Search by ID  4 Search, sort & reports
               5 Movement & restoration  6 Logout
Movement menu: 1 Move  2 History  3 Full log  4 Undo last move  5 Restoration queue  6 Back
```

## Known limits

- BST/AVL (listed under Module 3 in Phase-I) is not implemented.
- Passwords use one salted SHA-256, not a slow password hash such as bcrypt/argon2.
- Undo covers moves made in the current program run only (the stack is not saved).
- The priority formula's numbers are ours; only the Low/Medium/High banding idea comes from
  the English Heritage audit.
- Single operator, flat files, a few thousand records.
