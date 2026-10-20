# Changes from the previous version

## Update: three new artifact types (Weapon, Sculpture, Manuscript)
- `m2.h / m2.cpp`: `Weapon` (rust index), `Sculpture` (crack index) and `Manuscript` (decay index), each deriving
  from `Artifact` with the same condition-times-index penalty as pottery. They also derive from `Restorable`
  (an addition to the supplied classes, so they get conservation advice like pottery and coins);
  `serialize()` and `parseArtifact()` handle `WEAPON|`, `SCULPTURE|` and `MANUSCRIPT|` lines (7 fields each).
- `main.cpp`: Add Artifact now offers 6 types and asks for the right index.
- `m3.cpp`: the type filter lists all seven types.
- Tests: 94 unit checks (was 74) and 55 end-to-end checks (was 41), both clean under AddressSanitizer + UBSan.
- Existing data files keep working; the new line types are only written when you add those artifacts.

## Bugs fixed
1. **Menu loop** (old `m1.cpp`): `while (choice != 5)` ended the artifact menu after *Track Movement*,
   and *Logout* (6) never left it. Now `choice != 6` in `main.cpp`. Test: `run_tests.sh` section 2.
2. **Movement log format**: `m4.c` wrote `MOVE|id|from|to|user|reason|timestamp` (7 fields) but `m4.cpp`
   expected `id|from|to|date|by|reason` (6 fields), so the movement report skipped every real line.
   Module 3 now reads exactly what the movement module writes. Test: sections 7 and `m3_harness` [7].

## Structure
3. **Real headers, no `#include "x.cpp"`.** Each module is a `.h` + `.cpp` (or `.c`), built by `make`.
4. **Empty `m3.cpp` filled**: the search module now lives in `m3.cpp` (it was `m4.cpp`); `m4.c` stays movement.
5. `ArtifactStore` moved out of the hub into `m1.h/m1.cpp`; the bridge functions into `bridge.cpp`;
   `main()` is alone in `main.cpp`. The test harness no longer needs `#define main`.

## Pure C where the syllabus wants C
6. Hash table, stable merge sort and the BFS/DFS graph are now **pure C** (`m3_core.c`).
   `m3.cpp` wraps them (RAII classes, exceptions). Because C's `merge_sort` copies raw bytes, the C++
   wrapper sorts an array of indexes, never the `std::string`-holding objects themselves.
7. Duplicate check while loading `artifacts.txt` uses `unordered_set` (was a linear scan: O(n²)).

## Phase-I items that were missing, now built
8. **Undo stack** (`m4.c`): Undo Last Move reverses the latest move of the session and logs the reversal.
9. **Restoration priority queue** (`m4.c`): a binary max-heap lists the next artifacts to restore, with advice.
10. **`Restorable` mixin** (`m2.h`): pottery and coins derive from `Artifact` *and* `Restorable`
    (multiple inheritance); with `AncientCoin` this gives hybrid inheritance.
11. **Operator overloading**: `<`, `>`, `==`, `<<` on `Artifact` (priority sorting uses `<` / `>`).
12. **`this` pointer**: `setLocation` returns `*this`.
13. **Passwords** are salted SHA-256 (`m0.cpp`, checked against the standard test vectors). Old plain-text
    `users.txt` lines are upgraded automatically on first use. `users.txt` shipped here is already upgraded.

## Tests
- `tests/m3_harness.cpp`: 94 checks (was 38 originally): the old ones plus the C core called directly, the
  class hierarchy, operators, and passwords.
- `tests/run_tests.sh`: 55 end-to-end checks that read the real output and data files (the old script only
  logged output). Both also ran clean under AddressSanitizer + UBSan.

## Behaviour notes
- The merge sort is now a little slower than `std::stable_sort` (about 2x at 5,000 items) because the C core
  sorts through an index array and a comparison callback. Hash lookup is still ~380x faster than a linear scan.
- The menu option formerly named "Track Artifact Movement" is now "Movement & Restoration".
