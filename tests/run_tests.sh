#!/usr/bin/env bash
# run_tests.sh - end-to-end tests for ArtiTrack. Run from the repo root after `make`:
#     bash tests/run_tests.sh
# Every test feeds scripted keystrokes to ./artitrack inside a fresh scratch folder and
# checks the OUTPUT and the DATA FILES. Prints PASS/FAIL per check; exits 1 on any failure.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/artitrack"
HARNESS="$ROOT/m3_harness"
[ -x "$APP" ] || { echo "Build first:  make"; exit 2; }

PASS=0; FAIL=0
ok()   { echo "  PASS  $1"; PASS=$((PASS+1)); }
bad()  { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
has()  { if grep -qF -- "$2" "$1"; then ok "$3"; else bad "$3   (missing: $2)"; fi; }
hasnt(){ if grep -qF -- "$2" "$1"; then bad "$3   (found: $2)"; else ok "$3"; fi; }

fresh() {                     # fresh scratch folder with the 17-artifact test data
    W="$(mktemp -d)"; cd "$W"
    cp "$ROOT/tests/test_artifacts.txt" artifacts.txt
}
login() { printf '2\ntester\npass1\n'; }          # menu keystrokes: Login as tester
setup_user() { printf '1\ntester\npass1\n'; }     # menu keystrokes: Register tester

echo "== 1. Passwords =="
fresh
{ setup_user; printf '3\n'; } | "$APP" > o.log 2>&1
has o.log "Registration successful" "register works"
hasnt users.txt "pass1" "users.txt does not contain the plain password"
has users.txt "sha256\$" "users.txt holds a salted SHA-256 record"
printf '2\ntester\nwrong\n3\n' | "$APP" > o.log 2>&1
has o.log "Invalid username or password" "wrong password is rejected"
printf '2\ntester\npass1\n6\n3\n' | "$APP" > o.log 2>&1
has o.log "Welcome, tester" "right password logs in"
printf 'olduser oldpass\n' > users.txt            # a legacy plain-text account
printf '2\nolduser\noldpass\n6\n3\n' | "$APP" > o.log 2>&1
has o.log "Welcome, olduser" "legacy plain-text account still logs in"
has o.log "upgraded to hashed" "legacy account is upgraded automatically"
hasnt users.txt "oldpass" "plain text is gone from users.txt after the upgrade"

echo "== 2. Hub menu: Logout really logs out (this was a bug) =="
fresh
{ setup_user; login; printf '5\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
# after movement menu "Back" (6) we must still be in the artifact menu, and 6 must log out
has o.log "Logged out." "Logout (6) leaves the artifact menu after using the movement menu"
last_menu=$(grep -n "===== Artifact Menu" o.log | wc -l)
[ "$last_menu" -eq 2 ] && ok "artifact menu shown exactly twice (before and after the movement menu)" || bad "artifact menu shown $last_menu times, expected 2"

echo "== 3. Add artifact, persistence across restarts =="
fresh
rm artifacts.txt
{ setup_user; login; printf '1\n1\nP9\nTest Jar\n1200\n4\nDelhi\n0.6\n2\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Artifact saved successfully: P9 (Pottery, Test Jar) at Delhi" "add pottery prints the operator<< summary"
has artifacts.txt "POTTERY|P9|Test Jar|1200|" "artifact written to artifacts.txt"
{ login; printf '3\nP9\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "1 saved artifact(s) loaded" "artifact reloaded on the next run"
has o.log "Advice: Porous body" "search shows restoration advice for pottery"

echo "== 4. Movement, history, undo, log format =="
fresh
{ setup_user; login; printf '5\n1\nPT01\nStore Room B\nCleaning\n2\nPT01\n4\n1\n3\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Moved Harappan Jar (PT01)" "move succeeds"
has o.log "Delhi Museum  ->  Store Room B" "history shows the move"
has o.log "Undone: PT01 is back at \"Delhi Museum\"" "undo puts it back"
grep -q "^POTTERY|PT01|Harappan Jar|4500|3.500000|Delhi Museum|" artifacts.txt && ok "artifacts.txt shows PT01 back at Delhi Museum" || bad "artifacts.txt location after undo"
n=$(grep -c '^MOVE|PT01|' movements.txt)
[ "$n" -eq 2 ] && ok "movements.txt has the move AND the logged reversal (2 lines)" || bad "movements.txt has $n lines for PT01, expected 2"
has movements.txt "MOVE|PT01|Store Room B|Delhi Museum|tester|UNDO of move to Store Room B|" "undo line follows the 7-field MOVE format"
{ login; printf '5\n4\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Nothing to undo" "undo with an empty stack says so"
{ login; printf '5\n1\nNOPE\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Artifact not found." "moving an unknown artifact is refused"

echo "== 5. Restoration priority queue (heap) =="
fresh
{ setup_user; login; printf '5\n5\n4\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
first=$(grep -E '^ +1\. ' o.log | awk '{print $2}')
second=$(grep -E '^ +2\. ' o.log | awk '{print $2}')
third=$(grep -E '^ +3\. ' o.log | awk '{print $2}')
[ "$first/$second/$third" = "PT03/PT01/A004" ] && ok "top 3 by priority: PT03, PT01, A004" || bad "heap order was $first/$second/$third, expected PT03/PT01/A004"
has o.log "Advice: Porous body" "pottery advice shown in the queue"
has o.log "Advice: Check for bronze disease" "bronze coin advice shown in the queue"

echo "== 6. Search, sort, reports =="
fresh
{ setup_user; login; printf '4\n1\nA001\n3\n3\n2\n4\n3\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Era: Ancient | Priority score: 78.50" "find by ID uses the hash index"
has o.log "Sorted by priority, descending (17)" "sorted listing runs"
row=$(grep -A2 "Sorted by priority, descending" o.log | tail -1 | awk '{print $1}')
[ "$row" = "PT03" ] && ok "highest priority first is PT03" || bad "first sorted row was $row"
has o.log "No movements recorded yet" "movement report with no log says so"
{ login; printf '4\n1\nZZZ\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Not found: no artifact has the ID 'ZZZ'" "unknown ID -> ArtifactNotFoundException message"

echo "== 7. Movement report reads the REAL log (formats now match) =="
fresh
cp "$ROOT/tests/test_movements.txt" movements.txt
{ setup_user; login; printf '4\n4\n3\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Warning: skipped 3 malformed line(s) in movements.txt" "3 bad lines skipped with a warning"
a1=$(grep -E '^ +A001 +[0-9]+$' o.log | awk '{print $2}')
[ "$a1" = "3" ] && ok "A001 counted 3 moves from the real log" || bad "A001 moves: '$a1'"
# move through the app, then the report must include it
{ login; printf '5\n1\nC001\nField Camp\nTest\n6\n4\n4\n3\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
c1=$(grep -E '^ +C001 +[0-9]+$' o.log | awk '{print $2}')
[ "$c1" = "1" ] && ok "a move made in the app shows up in the movement report (modules agree on the format)" || bad "C001 moves: '$c1'"

echo "== 8. Location network (BFS / DFS) =="
fresh
cp "$ROOT/tests/test_network.txt" network.txt
cp "$ROOT/tests/test_movements.txt" movements.txt
{ setup_user; login; printf '4\n5\n3\nDelhi Museum\nKolkata Annex\n4\nDelhi Museum\n5\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Fewest transfers: 2" "BFS Delhi Museum -> Kolkata Annex is 2 transfers"
has o.log "Delhi Museum -> Patna Vault -> Kolkata Annex" "BFS prints the route"
has o.log "separate cluster(s)" "DFS reports cluster count"
fresh
mkdir network.txt                                      # a folder where the file should be
{ setup_user; login; printf '4\n5\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "File error: could not read the file 'network.txt'" "unreadable network file is reported, not a crash"
has o.log "Logged out." "program keeps running after the file error"

echo "== 9. Empty collection =="
fresh; rm artifacts.txt
{ setup_user; login; printf '2\n4\n4\n1\n5\n5\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "No artifacts saved yet." "display and heap handle an empty collection"
has o.log "Logged out." "no crash with zero artifacts"

echo "== 10. New types: Weapon, Sculpture, Manuscript =="
fresh; rm artifacts.txt
{ setup_user; login; printf '1\n4\nW1\nIron Sword\n800\n3\nDelhi\n0.8\n1\n5\nS1\nStone Lion\n1500\n5\nPatna\n0.2\n1\n6\nM1\nPalm Leaf\n600\n6\nKolkata\n0.9\n4\n2\n1\n5\n6\n5\n5\n3\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "Artifact saved successfully: W1 (Weapon, Iron Sword) at Delhi" "add Weapon"
has o.log "Artifact saved successfully: S1 (Sculpture, Stone Lion) at Patna" "add Sculpture"
has o.log "Artifact saved successfully: M1 (Manuscript, Palm Leaf) at Kolkata" "add Manuscript"
has artifacts.txt "WEAPON|W1|Iron Sword|800|3.000000|Delhi|0.800000" "Weapon line in artifacts.txt"
has artifacts.txt "SCULPTURE|S1|Stone Lion|1500|5.000000|Patna|0.200000" "Sculpture line in artifacts.txt"
has artifacts.txt "MANUSCRIPT|M1|Palm Leaf|600|6.000000|Kolkata|0.900000" "Manuscript line in artifacts.txt"
has o.log "Type: Weapon" "filter by type lists the Weapon"
first=$(grep -E '^ +1\. ' o.log | awk '{print $2}')
second=$(grep -E '^ +2\. ' o.log | awk '{print $2}')
third=$(grep -E '^ +3\. ' o.log | awk '{print $2}')
[ "$first/$second/$third" = "W1/S1/M1" ] && ok "restoration order W1 (68.00), S1 (47.50), M1 (43.00)" || bad "order was $first/$second/$third"
has o.log "Advice: Heavy rust" "weapon advice in the restoration queue"
has o.log "Advice: Stable: dust gently" "sculpture advice in the restoration queue"
has o.log "Advice: Advanced decay" "manuscript advice in the restoration queue"
{ login; printf '2\n3\nM1\n6\n3\n'; } | "$APP" > o.log 2>&1
has o.log "3 saved artifact(s) loaded" "all three new types reload on the next run"
has o.log "Type: Manuscript | Decay Index: 0.9" "Manuscript displays its decay index after a reload"
{ login; printf '5\n1\nW1\nStore Room B\nCleaning\n6\n6\n3\n'; } | "$APP" > o.log 2>&1
grep -q "^WEAPON|W1|Iron Sword|800|3.000000|Store Room B|0.800000" artifacts.txt && ok "moving a Weapon keeps its rust index in the saved line" || bad "weapon line after a move"

echo "== 11. Unit/harness checks =="
if [ -x "$HARNESS" ]; then
    H="$(mktemp -d)"; cd "$H"
    "$HARNESS" > h.log 2>&1 && ok "m3_harness: $(grep -o '[0-9]* passed, [0-9]* failed' h.log)" || bad "m3_harness reported failures (see $H/h.log)"
else
    echo "  (skipped: run  make m3_harness  first)"
fi

echo
echo "===== $PASS passed, $FAIL failed ====="
[ "$FAIL" -eq 0 ]
